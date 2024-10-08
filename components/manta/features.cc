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

// Enables Scanner Prod Server
BASE_FEATURE(kScannerUseProdServer,
             "ScannerUseProdServer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables SeaPen Prod Server
BASE_FEATURE(kSeaPenUseProdServer,
             "SeaPenUseProdServer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Mahi Prod Server
BASE_FEATURE(kMahiUseProdServer,
             "MahiUseProdServer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Walrus Prod Server
BASE_FEATURE(kWalrusUseProdServer,
             "WalrusUseProdServer",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsMantaServiceEnabled() {
  return base::FeatureList::IsEnabled(kMantaService);
}

bool IsOrcaUseProdServerEnabled() {
  return base::FeatureList::IsEnabled(kOrcaUseProdServer);
}

bool IsScannerUseProdServerEnabled() {
  return base::FeatureList::IsEnabled(kScannerUseProdServer);
}

bool IsSeaPenUseProdServerEnabled() {
  return base::FeatureList::IsEnabled(kSeaPenUseProdServer);
}

bool IsMahiUseProdServerEnabled() {
  return base::FeatureList::IsEnabled(kMahiUseProdServer);
}

bool IsWalrusUseProdServerEnabled() {
  return base::FeatureList::IsEnabled(kWalrusUseProdServer);
}

}  // namespace manta::features
