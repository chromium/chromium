// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MODEL_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MODEL_H_

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace growth {

enum class Slot { kDemoModeApp = 0 };

// Dictionary of supported targetings. For example:
// { "demoModeTargeting" : {...} }
using Targeting = base::Value::Dict;

// List of `Targeting`.
using Targetings = base::Value::List;

// Dictionary of supported payloads. For example:
// {
//   "demoMode": {
//     "attractionLoop": {
//       "videoSrcLang1": "/asset/lang1.mp4",
//       "videoSrcLang2": "/asset/lang2.mp4"
//     }
//   }
// }
using Payload = base::Value::Dict;

// Dictionary of Campaign. For example:
// {
//    "id": 1,
//    "targetings": {...}
//    "payload": {...}
// }
using Campaign = base::Value::Dict;

// List of campaigns.
using Campaigns = base::Value::List;

// Lists of campaigns keyed by the targeted slot. The key is the slot ID in
// string. For example:
// {
//   "0": [...]
//   "1": [...]
// }
using CampaignsPerSlot = base::Value::Dict;

// All campaigns including proactive and reactive campaigns. For example:
// {
//   "proactiveCampaigns" : {
//     "0": [...],
//     "1": [...]
//   },
//   "reactiveCampaigns" : {
//     "3": [...],
//     "4": [...]
//   },
// }
using CampaignsStore = base::Value::Dict;

const CampaignsPerSlot* GetProactiveCampaigns(
    const CampaignsStore* campaigns_store);

const CampaignsPerSlot* GetReactiveCampaigns(
    const CampaignsStore* campaigns_store);

const Campaigns* GetCampaignsBySlot(const CampaignsPerSlot* campaigns_per_slot,
                                    Slot slot);

const Targetings* GetTargetings(const Campaign* campaign);

const Payload* GetPayloadBySlot(const Campaign* campaign, Slot slot);

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MODEL_H_
