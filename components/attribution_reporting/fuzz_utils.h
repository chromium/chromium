// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_FUZZ_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_FUZZ_UTILS_H_

#include "components/attribution_reporting/max_event_level_reports.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace attribution_reporting {

auto AnyMaxEventLevelReports() {
  return fuzztest::ConstructorOf<MaxEventLevelReports>(
      fuzztest::InRange(0, static_cast<int>(MaxEventLevelReports::Max())));
}

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_FUZZ_UTILS_H_
