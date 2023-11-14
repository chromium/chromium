// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_GROWTH_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_GROWTH_METRICS_H_

namespace growth {

// These enum values represent user-facing errors in the campaigns loading and
// matching flow. Entries should not be renumbered and numeric values should
// never be reused. Please keep in sync with "CampaignsManagerError" in
// src/tools/metrics/histograms/enums.xml.
enum class CampaignsManagerError {
  kCampaignsComponentLoadFail = 0,
  kCampaignsFileLoadFail = 1,
  kCampaignsParsingFail = 2,
  kUserPrefUnavailableAtMatching = 3,
  kInvalidCampaign = 4,
  kInvalidTargeting = 5,

  kMaxValue = kInvalidTargeting,
};

// Records errors encountered during the campaigns loading and matching flow.
void RecordCampaignsManagerError(CampaignsManagerError error_code);

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_GROWTH_METRICS_H_
