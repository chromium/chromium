// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/safe_browsing_unverified_ruleset_dealer.h"

#include <string_view>

#include "components/subresource_filter/core/browser/subresource_filter_constants.h"

namespace subresource_filter {

std::string_view SafeBrowsingUnverifiedRulesetDealer::GetFilterTag() const {
  return kSafeBrowsingRulesetConfig.filter_tag;
}

}  // namespace subresource_filter
