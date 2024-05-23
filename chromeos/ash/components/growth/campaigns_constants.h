// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONSTANTS_H_

#include <optional>
#include <string>

class GURL;

namespace growth {

// List of events growth campaign supports.
enum class CampaignEvent {
  kImpression = 0,
  // Dismissed by user explicitly, e.g. click a button in the UI.
  kDismissed,
  kAppOpened,
  kEvent
};

// TODO: b/341955045 - Separate for UIEvent and AppOpenedEvent.
std::string GetEventName(CampaignEvent event, const std::string& id);

// Returns the app group id by individual app id.
// E.g. Gmail PWA and ARC apps could be grouped by `Gmail` group id.
// Some campaigns may use the app group id to do configuration.
std::optional<std::string> GetAppGroupId(const std::string& app_id);

// Returns the app group id by URL.
// E.g. Gmail website can be grouped with other Gmail PWA and ARC apps by
// `Gmail` group id. Some campaigns may use the app group id to do
// configuration.
std::optional<std::string> GetAppGroupId(const GURL& url);

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONSTANTS_H_
