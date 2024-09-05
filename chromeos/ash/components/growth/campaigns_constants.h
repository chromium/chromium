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
  kAppOpened,
  kEvent,
  kGroupImpression,
  kGroupDismissed
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONSTANTS_H_
