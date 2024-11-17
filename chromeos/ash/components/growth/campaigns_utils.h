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

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string ToString(bool value);

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_UTILS_H_
