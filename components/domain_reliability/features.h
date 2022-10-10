// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_FEATURES_H_
#define COMPONENTS_DOMAIN_RELIABILITY_FEATURES_H_

#include "base/feature_list.h"
#include "components/domain_reliability/domain_reliability_export.h"

namespace domain_reliability {
namespace features {

// Partitions Domain Reliability beacons and upload by NetworkIsolationKey.
DOMAIN_RELIABILITY_EXPORT BASE_DECLARE_FEATURE(kPartitionDomainReliabilityByNetworkIsolationKey);

}  // namespace features
}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_FEATURES_H_