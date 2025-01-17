// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/growth_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/growth/campaigns_utils.h"

namespace growth {

namespace {

inline constexpr char kCampaignsManagerErrorHistogramName[] =
    "Ash.Growth.CampaignsManager.Error";

inline constexpr char kGetCampaignBySlotAttemptHistogramName[] =
    "Ash.Growth.CampaignsManager.GetCampaignBySlot.Attempt";

inline constexpr char kGetCampaignBySlotHistogramName[] =
    "Ash.Growth.CampaignsManager.GetCampaignBySlot";

inline constexpr char
    kCampaignsComponentDownloadDurationSessionStartHistogram[] =
        "Ash.Growth.CampaignsComponent.DownloadDurationSessionStart";

inline constexpr char kCampaignsComponentDownloadDurationInOobeHistogram[] =
    "Ash.Growth.CampaignsComponent.DownloadDurationInOobe";

inline constexpr char kCampaignsComponentReadDurationHistogram[] =
    "Ash.Growth.CampaignsComponent.ParseDuration";

inline constexpr char kCampaignMatchDurationHistogram[] =
    "Ash.Growth.CampaignsManager.MatchDuration";

constexpr char kGetCampaignHistogramName[] =
    "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns%d";

std::string GetCampaignHistogramName(int campaign_id) {
  // E.g. "Ash.Growth.CampaignsManager.GetCampaignBySlot.Campaigns500".
  return base::StringPrintf(kGetCampaignHistogramName,
                            GetHistogramMaxCampaignId(campaign_id));
}

void RecordCampaignFetched(int campaign_id) {
  const std::string histogram_name = GetCampaignHistogramName(campaign_id);
  base::UmaHistogramSparse(histogram_name, campaign_id);
}

}  // namespace

void RecordCampaignsManagerError(CampaignsManagerError error) {
  base::UmaHistogramEnumeration(kCampaignsManagerErrorHistogramName, error);
}

void RecordGetCampaignBySlotAttempt(Slot slot) {
  base::UmaHistogramEnumeration(kGetCampaignBySlotAttemptHistogramName, slot);
}

void RecordGetCampaignBySlot(Slot slot, int campaign_id) {
  base::UmaHistogramEnumeration(kGetCampaignBySlotHistogramName, slot);
  RecordCampaignFetched(campaign_id);
}

void RecordCampaignsComponentDownloadDuration(const base::TimeDelta duration,
                                              bool in_oobe) {
  base::UmaHistogramLongTimes100(
      in_oobe ? kCampaignsComponentDownloadDurationInOobeHistogram
              : kCampaignsComponentDownloadDurationSessionStartHistogram,
      duration);
}

void RecordCampaignsComponentReadDuration(const base::TimeDelta duration) {
  // We don't normally expect the duration is longer than 10s. If this limit is
  // exceeded, then the metric would fall into an overflow bucket.
  base::UmaHistogramTimes(kCampaignsComponentReadDurationHistogram, duration);
}

void RecordCampaignMatchDuration(const base::TimeDelta duration) {
  // We don't normally expect the duration is longer than 10s. If this limit is
  // exceeded, then the metric would fall into an overflow bucket.
  base::UmaHistogramTimes(kCampaignMatchDurationHistogram, duration);
}

}  // namespace growth
