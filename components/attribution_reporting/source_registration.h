// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {
class StorableSource;
}  // namespace content

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) SourceRegistration {
 public:
  static base::expected<SourceRegistration, mojom::SourceRegistrationError>
  Parse(base::Value::Dict, url::Origin reporting_origin);

  ~SourceRegistration();

  SourceRegistration(const SourceRegistration&);
  SourceRegistration& operator=(const SourceRegistration&);

  SourceRegistration(SourceRegistration&&);
  SourceRegistration& operator=(SourceRegistration&&);

 private:
  // Allow efficient moves out of this type's fields without defining extractor
  // methods for each one.
  friend ::content::StorableSource;

  SourceRegistration();

  uint64_t source_event_id_;
  url::Origin destination_;
  url::Origin reporting_origin_;
  absl::optional<base::TimeDelta> expiry_;
  absl::optional<base::TimeDelta> event_report_window_;
  absl::optional<base::TimeDelta> aggregatable_report_window_;
  int64_t priority_;
  FilterData filter_data_;
  absl::optional<uint64_t> debug_key_;
  AggregationKeys aggregation_keys_;
  bool debug_reporting_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
