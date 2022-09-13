// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_configs.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_starter_heuristic_config.h"

namespace autofill_assistant {

namespace {

// Shopping and coupons share most of their config, except the intent. We also
// include a denylist of domains just for performance reasons, i.e., we
// pre-exclude a somewhat arbitrary list of high-traffic domains that are not
// relevant for the given intents. This list should be updated occasionally to
// ensure that we keep filtering most of the noise.
const char kSharedShoppingConfigWithoutIntent[] = R"(
    "denylistedDomains": ["google.com", "facebook.com", "ampproject.org",
                        "pornhub.com", "xnxx.com", "xvideos.com", "twitter.com",
                        "instagram.com", "craigslist.org", "yahoo.com",
                        "googleadservices.com", "youtube.com",
                        "zillow.com", "wikipedia.org", "xhamster.com",
                        "pinterest.com", "reddit.com", "indeed.com",
                        "dailymail.co.uk", "weather.com", "mlb.com",
                        "live.com", "realtor.com", "trulia.com",
                        "ca.gov", "pch.com", "paypal.com", "office.com",
                        "espn.com"],
    "heuristics": [
      {
        "conditionSet": {
          "schemes":["https"],
          "urlMatches":
            "(?i)cart|trolley|basket|checkout|fulfil+ment|bag|shipping|pay|buy"
        }
      },
      {
        "conditionSet":{
        "urlPrefix":
          "https://www.jegs.com/webapp/wcs/stores/servlet/OrderItemDisplay"
        }
      }
    ],
    "enabledInCustomTabs":true,
    "enabledInRegularTabs":false,
    "enabledInWeblayer":false,
    "enabledForSignedOutUsers":true,
    "enabledWithoutMsbb":false
  )";

}  // namespace

namespace launched_configs {

LaunchedStarterHeuristicConfig* GetOrCreateShoppingConfig() {
  static base::NoDestructor<LaunchedStarterHeuristicConfig> shopping_config(
      features::kAutofillAssistantInCCTTriggering, /* parameters = */
      base::StrCat({"{", R"("intent": "SHOPPING_ASSISTED_CHECKOUT",)",
                    kSharedShoppingConfigWithoutIntent, "}"}),
      /* country_codes = */ base::flat_set<std::string>{"gb", "us"});
  return shopping_config.get();
}

LaunchedStarterHeuristicConfig* GetOrCreateCouponsConfig() {
  static base::NoDestructor<LaunchedStarterHeuristicConfig> coupons_config(
      features::kAutofillAssistantInCCTTriggering,
      /* parameters = */
      base::StrCat({"{", R"("intent": "FIND_COUPONS",)",
                    kSharedShoppingConfigWithoutIntent, "}"}),
      /* country_codes = */ base::flat_set<std::string>{"us"});
  return coupons_config.get();
}

}  // namespace launched_configs
}  // namespace autofill_assistant
