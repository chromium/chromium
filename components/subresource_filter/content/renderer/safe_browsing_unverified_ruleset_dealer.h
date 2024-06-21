// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SAFE_BROWSING_UNVERIFIED_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SAFE_BROWSING_UNVERIFIED_RULESET_DEALER_H_

#include <string_view>

#include "components/subresource_filter/content/shared/renderer/unverified_ruleset_dealer.h"

namespace subresource_filter {

class SafeBrowsingUnverifiedRulesetDealer : public UnverifiedRulesetDealer {
 public:
  SafeBrowsingUnverifiedRulesetDealer() = default;

  SafeBrowsingUnverifiedRulesetDealer(
      const SafeBrowsingUnverifiedRulesetDealer&) = delete;
  SafeBrowsingUnverifiedRulesetDealer& operator=(
      const SafeBrowsingUnverifiedRulesetDealer&) = delete;

  ~SafeBrowsingUnverifiedRulesetDealer() override = default;

 private:
  std::string_view GetFilterTag() const override;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SAFE_BROWSING_UNVERIFIED_RULESET_DEALER_H_
