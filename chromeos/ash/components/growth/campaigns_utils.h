// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_UTILS_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"

class GURL;

namespace growth {

// A util function to add the `kGrowthCampaignsEventNamePrefix`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string GetGrowthCampaignsEventNamePrefix();

// TODO: b/341955045 - Separate for UIEvent and AppOpenedEvent.
// Returns event name from provided `event` and `id`.
// NOTE: This event name is not prefixed by the
// `kGrowthCampaignsEventNamePrefix`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::string GetEventName(CampaignEvent event, const std::string& id);

// Returns the app group id by individual app id.
// E.g. Gmail PWA and ARC apps could be grouped by `Gmail` group id.
// Some campaigns may use the app group id to do configuration.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::optional<std::string> GetAppGroupId(const std::string& app_id);

// Returns the app group id by URL.
// E.g. Gmail website can be grouped with other Gmail PWA and ARC apps by
// `Gmail` group id. Some campaigns may use the app group id to do
// configuration.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_UTILS)
std::optional<std::string> GetAppGroupId(const GURL& url);

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_UTILS_H_
