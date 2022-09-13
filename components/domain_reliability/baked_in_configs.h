// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_BAKED_IN_CONFIGS_H_
#define COMPONENTS_DOMAIN_RELIABILITY_BAKED_IN_CONFIGS_H_

#include "components/domain_reliability/domain_reliability_export.h"

namespace domain_reliability {

// NULL-terminated array of pointers to JSON-encoded Domain Reliability
// configurations. Read by DomainReliabilityMonitor::AddBakedInConfigs.
DOMAIN_RELIABILITY_EXPORT extern const char* const kBakedInJsonConfigs[];

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_BAKED_IN_CONFIGS_H_
