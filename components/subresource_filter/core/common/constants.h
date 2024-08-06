// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_CONSTANTS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_CONSTANTS_H_

#include "components/subresource_filter/core/common/ruleset_config.h"

namespace subresource_filter {

// The config used to identify the original Safe Browsing ruleset for the
// RulesetService. Encompasses a ruleset tag and top level directory name where
// the ruleset should be stored.
extern const RulesetConfig kSafeBrowsingRulesetConfig;

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_CONSTANTS_H_
