// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_UTILS_H_

#include <map>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/strings/cstring_view.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"

class GURL;

namespace growth {

// A util function to add the `kGrowthCampaignsEventNamePrefix`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string_view GetGrowthCampaignsEventNamePrefix();

// TODO: b/341955045 - Separate for UIEvent and AppOpenedEvent.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string GetEventName(CampaignEvent event, std::string_view id);

// Returns the app group id by individual app id.
// E.g. Gmail PWA and ARC apps could be grouped by `Gmail` group id.
// Some campaigns may use the app group id to do configuration.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string_view GetAppGroupId(std::string_view app_id);

// Returns the app group id by URL.
// E.g. Gmail website can be grouped with other Gmail PWA and ARC apps by
// `Gmail` group id. Some campaigns may use the app group id to do
// configuration.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string_view GetAppGroupId(const GURL& url);

// Returns the base conditions to query feature engagement framework.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::map<std::string, std::string> CreateBasicConditionParams();

// Returns the query string to check the impression/dismissal cap.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string CreateConditionParamForCap(base::cstring_view campaign_type,
                                       int id,
                                       base::cstring_view event_type,
                                       int cap);

// Returns the max `campaign_id` (exclusive) can be recorded in the histogram.
// Campaign will be logged in a histogram named by rounding the `campaign_id`
// to the next five hundred. For examples:
//   `campaign_id`: 0 =>
//   "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns500" `campaign_id`:
//   100 => "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns500"
//   `campaign_id`: 499 =>
//   "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns500" `campaign_id`:
//   500 => "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns1000"
//   `campaign_id`: 501 =>
//   "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns1000"
//   `campaign_id`: 1000 =>
//   "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns1500"
//   `campaign_id`: 9999 =>
//   "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns10000"
//   `campaign_id`: 10000 =>
//   "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns10500"
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
int GetHistogramMaxCampaignId(int campaign_id);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string ToString(bool value);

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_UTILS_H_
