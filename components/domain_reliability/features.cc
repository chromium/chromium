// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/features.h"

namespace domain_reliability {
namespace features {

DOMAIN_RELIABILITY_EXPORT extern const base::Feature
    kPartitionDomainReliabilityByNetworkIsolationKey{
        "PartitionDomainReliabilityByNetworkIsolationKey",
        base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace domain_reliability
