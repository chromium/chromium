// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/features.h"

#include "base/feature_list.h"

namespace manta::features {

BASE_FEATURE(kMantaService, "MantaService", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Orca Prod Server
BASE_FEATURE(kOrcaUseProdServer,
             "OrcaUseProdServer",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsMantaServiceEnabled() {
  return base::FeatureList::IsEnabled(kMantaService);
}

bool IsOrcaUseProdServerEnabled() {
  return base::FeatureList::IsEnabled(kOrcaUseProdServer);
}

}  // namespace manta::features
