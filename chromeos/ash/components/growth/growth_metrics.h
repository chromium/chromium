// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_GROWTH_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_GROWTH_METRICS_H_

#include "base/component_export.h"
#include "chromeos/ash/components/growth/campaigns_model.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace growth {

// These enum values represent user-facing errors in the campaigns loading and
// matching flow. Entries should not be renumbered and numeric values should
// never be reused. Please keep in sync with "CampaignsManagerError" in
// src/tools/metrics/histograms/metadata/ash/enums.xml.
enum class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
    CampaignsManagerError {
      kCampaignsComponentLoadFail = 0,
      kCampaignsFileLoadFail = 1,
      kCampaignsParsingFail = 2,
      kUserPrefUnavailableAtMatching = 3,
      kInvalidCampaign = 4,
      kInvalidTargeting = 5,
      kInvalidSchedulingTargeting = 6,
      kInvalidScheduling = 7,
      kDemoModeAppVersionUnavailable = 8,
      kSerializingDemoModePayloadFail = 9,
      kOpenUrlParamsParsingFail = 10,
      kMissingActionType = 11,
      kInvalidAppTargeting = 12,
      kInvalidAnchorType = 13,
      kUnrecognizedBuiltInIcon = 14,
      kMissingCampaignId = 15,
      kInvalidAction = 16,
      kPerformActionFailed = 17,
      kInvalidEventTargetingCondition = 18,
      kInvalidEventTargetingConditionParam = 19,
      kNoOpendedWindowToAnchor = 20,
      kNoOpendedWindowWidgetToAnchor = 21,
      kNoRootViewToGetAnchorView = 22,
      kNudgePayloadInvalidImage = 23,
      kNudgePayloadMissingBody = 24,
      kNudgeAnchorViewNotFound = 25,
      kNotificationPayloadMissingIcon = 26,
      kNotificationPayloadInvalidIcon = 27,
      kNotificationPayloadInvalidButton = 28,
      kNotificationPayloadMissingButtonLabel = 29,
      kNotificationPayloadMissingButtonAction = 30,
      kInvalidNotificationPayload = 31,

      kMaxValue = kInvalidNotificationPayload,
    };

// Records errors encountered during the campaigns loading and matching flow.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
void RecordCampaignsManagerError(CampaignsManagerError error_code);

// Records how many times a campaign is fetched for the given `slot`.
void RecordGetCampaignBySlot(Slot slot);

void RecordCampaignsComponentDownloadDuration(const base::TimeDelta duration,
                                              bool in_oobe);

void RecordCampaignsComponentReadDuration(const base::TimeDelta duration);

void RecordCampaignMatchDuration(const base::TimeDelta duration);

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_GROWTH_METRICS_H_
