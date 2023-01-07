// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_CLEAR_MODE_H_
#define COMPONENTS_DOMAIN_RELIABILITY_CLEAR_MODE_H_

#include "components/domain_reliability/domain_reliability_export.h"

namespace domain_reliability {

// Argument to DomainReliabilityMonitor::ClearBrowsingData.
enum DomainReliabilityClearMode {
  // Clear accumulated beacons (which betray browsing history) but leave
  // registered contexts intact.
  CLEAR_BEACONS,

  // Clear registered contexts (which can act like cookies).
  CLEAR_CONTEXTS,

  MAX_CLEAR_MODE
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_CLEAR_MODE_H_
