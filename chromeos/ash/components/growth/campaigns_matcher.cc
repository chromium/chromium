// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_matcher.h"
#include "base/logging.h"

namespace growth {

CampaignsMatcher::CampaignsMatcher() = default;
CampaignsMatcher::~CampaignsMatcher() = default;

void CampaignsMatcher::SetCampaigns(const CampaignsPerSlot* proactiveCampaigns,
                                    const CampaignsPerSlot* reactiveCampaigns) {
  proactive_campaigns_ = proactiveCampaigns;
  reactive_campaigns_ = reactiveCampaigns;
}

const Campaign* CampaignsMatcher::GetCampaignBySlot(Slot slot) const {
  // TODO(b/298467438): Add demo mode targeting and select campaign for the
  // given `slot`.
  return nullptr;
}

}  // namespace growth
