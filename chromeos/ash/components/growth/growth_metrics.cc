// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/growth_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace growth {

namespace {

inline constexpr char kCampaignsManagerErrorHistogramName[] =
    "Ash.Growth.CampaignsManager.Error";

inline constexpr char kCampaignsComponentDownloadDurationHistogram[] =
    "Ash.Growth.CampaignsComponent.DownloadDuration";

inline constexpr char kCampaignsComponentReadDurationHistogram[] =
    "Ash.Growth.CampaignsComponent.ParseDuration";

inline constexpr char kCampaignMatchDurationHistogram[] =
    "Ash.Growth.CampaignsManager.MatchDuration";

}  // namespace

void RecordCampaignsManagerError(CampaignsManagerError error) {
  base::UmaHistogramEnumeration(kCampaignsManagerErrorHistogramName, error);
}

void RecordCampaignsComponentDownloadDuration(const base::TimeDelta duration) {
  base::UmaHistogramLongTimes100(kCampaignsComponentDownloadDurationHistogram,
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
