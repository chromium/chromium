// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/features.h"

namespace domain_reliability {
namespace features {

DOMAIN_RELIABILITY_EXPORT BASE_FEATURE(kPartitionDomainReliabilityByNetworkIsolationKey,
                                       "PartitionDomainReliabilityByNetworkIsolationKey",
                                       base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace domain_reliability
