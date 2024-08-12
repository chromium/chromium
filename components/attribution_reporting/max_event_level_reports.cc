// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/max_event_level_reports.h"

#include <optional>

#include "base/check.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;

bool IsMaxEventLevelReportsValid(int i) {
  return i >= 0 && i <= kMaxSettableEventLevelAttributionsPerSource;
}

int DefaultMaxEventLevelReports(SourceType source_type) {
  switch (source_type) {
    case SourceType::kNavigation:
      return 3;
    case SourceType::kEvent:
      return 1;
  }
}

}  // namespace

// static
base::expected<MaxEventLevelReports, SourceRegistrationError>
MaxEventLevelReports::Parse(const base::Value::Dict& dict,
                            SourceType source_type) {
  const base::Value* value = dict.Find(kMaxEventLevelReports);
  if (!value) {
    return MaxEventLevelReports(source_type);
  }

  ASSIGN_OR_RETURN(int i, ParseInt(*value), [](ParseError) {
    return SourceRegistrationError::kMaxEventLevelReportsValueInvalid;
  });

  if (!IsMaxEventLevelReportsValid(i)) {
    return base::unexpected(
        SourceRegistrationError::kMaxEventLevelReportsValueInvalid);
  }

  return MaxEventLevelReports(i);
}

// static
MaxEventLevelReports MaxEventLevelReports::Max() {
  return MaxEventLevelReports(kMaxSettableEventLevelAttributionsPerSource);
}

MaxEventLevelReports::MaxEventLevelReports(int max_event_level_reports)
    : max_event_level_reports_(max_event_level_reports) {
  CHECK(IsMaxEventLevelReportsValid(max_event_level_reports_));
}

MaxEventLevelReports::MaxEventLevelReports(SourceType source_type)
    : MaxEventLevelReports(DefaultMaxEventLevelReports(source_type)) {}

bool MaxEventLevelReports::SetIfValid(int max_event_level_reports) {
  if (!IsMaxEventLevelReportsValid(max_event_level_reports)) {
    return false;
  }
  max_event_level_reports_ = max_event_level_reports;
  return true;
}

void MaxEventLevelReports::Serialize(base::Value::Dict& dict) const {
  dict.Set(kMaxEventLevelReports, max_event_level_reports_);
}

}  // namespace attribution_reporting
