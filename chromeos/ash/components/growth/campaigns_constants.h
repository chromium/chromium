// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONSTANTS_H_

namespace growth {

// List of events growth campaign supports.
enum class CampaignEvent {
  kImpression = 0,
  // Dismissed by user explicitly, e.g. click a button in the UI.
  kDismissed,
  kAppOpened
};

// Only event name with this prefix can be processed by the Feature Engagement
// framework.
inline constexpr char kGrowthCampaignsEventNamePrefix[] =
    "ChromeOSAshGrowthCampaigns";

// All event names will be prefixed by `kGrowthCampaignsEventNamePrefix`.
// Campaign specific event names will be suffixed by campaign id.
// App specific event names will be suffixed by app id.
// E.g. `ChromeOSAshGrowthCampaigns_Impression_CampaignId`
//      `ChromeOSAshGrowthCampaigns_AppOpened_AppId`
inline constexpr char kCampaignEventNameImpression[] = "_Campaign%s_Impression";

inline constexpr char kCampaignEventNameDismissed[] = "_Campaign%s_Dismissed";

inline constexpr char kCampaignEventNameAppOpened[] = "_AppOpened_AppId_%s";

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONSTANTS_H_
