// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerConfig {
 public:
  static base::expected<TriggerConfig, mojom::SourceRegistrationError> Parse(
      const base::Value::Dict&);

  TriggerConfig();

  explicit TriggerConfig(mojom::TriggerDataMatching);

  ~TriggerConfig();

  TriggerConfig(const TriggerConfig&);
  TriggerConfig& operator=(const TriggerConfig&);

  TriggerConfig(TriggerConfig&&);
  TriggerConfig& operator=(TriggerConfig&&);

  mojom::TriggerDataMatching trigger_data_matching() const {
    return trigger_data_matching_;
  }

  // Serializes into the given dictionary iff
  // `features::kAttributionReportingTriggerConfig` is enabled.
  void Serialize(base::Value::Dict&) const;

  // Always serializes regardless of the above feature status.
  void SerializeForTesting(base::Value::Dict&) const;

 private:
  mojom::TriggerDataMatching trigger_data_matching_ =
      mojom::TriggerDataMatching::kModulus;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_
