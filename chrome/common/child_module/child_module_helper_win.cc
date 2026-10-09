// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_module/child_module_helper.h"

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/win/sid.h"
#include "components/base32/base32.h"
#include "crypto/hash.h"

namespace child_module {

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
