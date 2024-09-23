// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SUMMARY_BUCKETS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SUMMARY_BUCKETS_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/summary_operator.mojom-forward.h"

namespace attribution_reporting {

class MaxEventLevelReports;

// Controls the bucketization of event-level triggers.
// Corresponds to
// https://wicg.github.io/attribution-reporting-api/#summary-bucket-list
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) SummaryBuckets {
 public:
  static base::expected<SummaryBuckets, mojom::SourceRegistrationError> Parse(
      const base::Value::Dict&,
      MaxEventLevelReports);

  // Represents `[1, 2, ... max_event_level_reports]`.
  // `CHECK()`s that `max_event_level_reports` is positive.
  explicit SummaryBuckets(MaxEventLevelReports max_event_level_reports);

  // `CHECK()`s that `starts` is valid: Must be non-empty, all values > 0, and
  // length <= `MaxEventLevelReports::Max()`.
  explicit SummaryBuckets(base::flat_set<uint32_t> starts);

  ~SummaryBuckets();

  SummaryBuckets(const SummaryBuckets&);
  SummaryBuckets& operator=(const SummaryBuckets&);

  SummaryBuckets(SummaryBuckets&&);
  SummaryBuckets& operator=(SummaryBuckets&&);

  const base::flat_set<uint32_t>& starts() const { return starts_; }

  void Serialize(base::Value::Dict&) const;

  friend bool operator==(const SummaryBuckets&,
                         const SummaryBuckets&) = default;

 private:
  base::flat_set<uint32_t> starts_;
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<mojom::SummaryOperator, mojom::SourceRegistrationError>
ParseSummaryOperator(const base::Value::Dict&);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
void Serialize(mojom::SummaryOperator, base::Value::Dict&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SUMMARY_BUCKETS_H_
