// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_list_filter.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/win/conflicts/module_info.h"

namespace {

bool MatchesModuleGroup(const chrome::conflicts::ModuleGroup& module_group,
                        std::string_view module_basename_hash,
                        std::string_view module_code_id_hash) {
  // Now look at each module in the group in detail.
  for (const auto& module : module_group.modules()) {
    // A valid entry contains one of the basename and the code id.
    if (!module.has_basename_hash() && !module.has_code_id_hash())
      continue;

    // Skip this entry if it doesn't match the basename of the module.
    if (module.has_basename_hash() &&
        module.basename_hash() != module_basename_hash) {
      continue;
    }

    // Or the code id.
    if (module.has_code_id_hash() &&
        module.code_id_hash() != module_code_id_hash) {
      continue;
    }

    return true;
  }

  return false;
}

}  // namespace

ModuleListFilter::ModuleListFilter() = default;

ModuleListFilter::~ModuleListFilter() = default;

bool ModuleListFilter::Initialize(const base::FilePath& module_list_path) {
  DCHECK(!initialized_);

  std::string contents;
  initialized_ = base::ReadFileToString(module_list_path, &contents) &&
                 module_list_.ParseFromString(contents);

  return initialized_;
}

bool ModuleListFilter::IsAllowlisted(
    std::string_view module_basename_hash,
    std::string_view module_code_id_hash) const {
  DCHECK(initialized_);

  for (const auto& module_group : module_list_.allowlist().module_groups()) {
    if (MatchesModuleGroup(module_group, module_basename_hash,
                           module_code_id_hash)) {
      return true;
    }
  }

  return false;
}

bool ModuleListFilter::IsAllowlisted(const ModuleInfoKey& module_key,
                                     const ModuleInfoData& module_data) const {
  // Precompute the hash of the basename and of the code id.
  const std::string module_basename_hash =
      base::SHA1HashString(base::UTF16ToUTF8(
          base::i18n::ToLower(module_data.inspection_result->basename)));
  const std::string module_code_id_hash =
      base::SHA1HashString(GenerateCodeId(module_key));

  return IsAllowlisted(module_basename_hash, module_code_id_hash);
}

std::unique_ptr<chrome::conflicts::BlocklistAction>
ModuleListFilter::IsBlocklisted(const ModuleInfoKey& module_key,
                                const ModuleInfoData& module_data) const {
  DCHECK(initialized_);

  // Precompute the hash of the basename and of the code id.
  const std::string module_basename_hash =
      base::SHA1HashString(base::UTF16ToUTF8(
          base::i18n::ToLower(module_data.inspection_result->basename)));
  const std::string module_code_id_hash =
      base::SHA1HashString(GenerateCodeId(module_key));

  for (const auto& blocklist_module_group :
       module_list_.blocklist().module_groups()) {
    if (MatchesModuleGroup(blocklist_module_group.modules(),
                           module_basename_hash, module_code_id_hash)) {
      return std::make_unique<chrome::conflicts::BlocklistAction>(
          blocklist_module_group.action());
    }
  }

  return nullptr;
}
