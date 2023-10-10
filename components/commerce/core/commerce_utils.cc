// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_utils.h"

#include "base/feature_list.h"
#include "components/commerce/core/commerce_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace commerce {
bool UrlContainsDiscountUtmTag(const GURL& url) {
  std::string utm_source;
  std::string utm_medium;
  std::string utm_campaign;
  if (!net::GetValueForKeyInQuery(url, commerce::kUTMSourceLabel,
                                  &utm_source)) {
    return false;
  }
  if (!net::GetValueForKeyInQuery(url, commerce::kUTMMediumLabel,
                                  &utm_medium)) {
    return false;
  }
  if (!net::GetValueForKeyInQuery(url, commerce::kUTMCampaignLabel,
                                  &utm_campaign)) {
    return false;
  }
  return utm_source == commerce::kUTMSourceValue &&
         utm_medium == commerce::kUTMMediumValue &&
         utm_campaign == commerce::kUTMCampaignValueForDiscounts;
}

}  // namespace commerce
