// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"

#include <string_view>

#include "base/files/file_path.h"
#include "components/subresource_filter/core/common/ruleset_config.h"

namespace fingerprinting_protection_filter {

constexpr subresource_filter::RulesetConfig
    kFingerprintingProtectionRulesetConfig = {
        .filter_tag = std::string_view("fingerprinting_protection_filter"),
        .top_level_directory =
            FILE_PATH_LITERAL("Fingerprinting Protection Filter"),
        .uma_tag = std::string_view("FingerprintingProtection")};

constexpr base::FilePath::CharType kUnindexedRulesetDataFileName[] =
    FILE_PATH_LITERAL("filtering_rules");

}  // namespace fingerprinting_protection_filter
