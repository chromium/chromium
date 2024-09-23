// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/constants.h"

#include <string_view>

#include "base/files/file_path.h"
#include "components/subresource_filter/core/common/ruleset_config.h"

namespace subresource_filter {

constexpr RulesetConfig kSafeBrowsingRulesetConfig = {
    .filter_tag = std::string_view("subresource_filter"),
    .top_level_directory = FILE_PATH_LITERAL("Subresource Filter"),
    .uma_tag = std::string_view("SubresourceFilter")};

}  // namespace subresource_filter
