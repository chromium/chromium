// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_RULESET_CONFIG_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_RULESET_CONFIG_H_

#include <string_view>

#include "base/files/file_path.h"

namespace subresource_filter {

// Metadata used to configure where to write ruleset files and metrics for a
// particular instance of the RulesetService, which depends on which filter it
// is being used for.
// NOTE: The members on this struct should have static storage duration
// (e.g. be marked extern).
struct RulesetConfig {
  const std::string_view filter_tag;
  const base::FilePath::StringPieceType top_level_directory;
  const std::string_view uma_tag;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_RULESET_CONFIG_H_
