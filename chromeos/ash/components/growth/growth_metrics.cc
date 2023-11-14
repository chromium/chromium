// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/growth_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace growth {

namespace {

inline constexpr char kCampaignsManagerErrorHistogramName[] =
    "Ash.Growth.CampaignsManager.Error";

}  // namespace

void RecordCampaignsManagerError(CampaignsManagerError error) {
  base::UmaHistogramEnumeration(kCampaignsManagerErrorHistogramName, error);
}

}  // namespace growth
