// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_module/child_module_helper.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/win/sid.h"
#include "chrome/common/chrome_constants.h"
#include "components/base32/base32.h"
#include "crypto/hash.h"

namespace child_module {

namespace {

// Returns true if `user_path_component` is well-formed, as produced by
// `ComputeUserPathComponent()`. A component from a child process's command
// line is validated before it is used in a path.
bool IsValidUserPathComponent(const base::FilePath& user_path_component) {
  // The number of characters in a user path component: 80 bits in Base32
  // without padding.
  static constexpr size_t kUserPathComponentLength = 16;

  const base::FilePath::StringType& value = user_path_component.value();
  return value.size() == kUserPathComponentLength &&
         std::ranges::all_of(value, [](base::FilePath::CharType c) {
           return (c >= L'a' && c <= L'z') || (c >= L'2' && c <= L'7');
         });
}

}  // namespace

base::FilePath GetModulesDirForUser(const base::FilePath& base_version_dir,
                                    const base::FilePath& user_path_component) {
  if (base_version_dir.empty() ||
      !IsValidUserPathComponent(user_path_component)) {
    return base::FilePath();
  }
  return base_version_dir.Append(kModulesDirName).Append(user_path_component);
}

base::FilePath GetModulesDirForUser(const base::FilePath& user_path_component) {
  base::FilePath install_dir;
  if (!base::PathService::Get(base::DIR_EXE, &install_dir)) {
    return base::FilePath();
  }
  return GetModulesDirForUser(install_dir.AppendASCII(chrome::kChromeVersion),
                              user_path_component);
}

base::FilePath GetUserPathComponentFromRendererBinaryPath(
    const base::FilePath& renderer_binary_path) {
  // The binary is at <ModulesDir>\<Version>\<Binary>, where the final
  // component of <ModulesDir> is the user path component.
  base::FilePath user_path_component =
      renderer_binary_path.DirName().DirName().BaseName();
  if (!IsValidUserPathComponent(user_path_component)) {
    return base::FilePath();
  }
  return user_path_component;
}

base::FilePath ComputeUserPathComponent(
    const base::win::Sid& sid,
    const base::FilePath& canonical_user_data_dir) {
  if (canonical_user_data_dir.empty() ||
      !canonical_user_data_dir.IsAbsolute()) {
    return base::FilePath();
  }
  std::optional<std::wstring> sddl = sid.ToSddlString();
  if (!sddl) {
    return base::FilePath();
  }
  const std::wstring udd_str =
      base::ToLowerASCII(canonical_user_data_dir.NormalizePathSeparators()
                             .StripTrailingSeparators()
                             .value());

  // The UTF-16 code units of the key are hashed directly, without conversion.
  const std::wstring composite_key = base::StrCat({*sddl, L":", udd_str});
  const auto digest = crypto::hash::Sha256(base::as_byte_span(composite_key));
  auto hash_80bit = base::span(digest).first<10>();

  std::string raw_base32 = base32::Base32Encode(
      hash_80bit, base32::Base32EncodePolicy::OMIT_PADDING);
  return base::FilePath::FromASCII(base::ToLowerASCII(raw_base32));
}

}  // namespace child_module
