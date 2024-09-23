// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_CONFIG_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_CONFIG_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableTriggerConfig {
 public:
  static base::expected<AggregatableTriggerConfig,
                        mojom::TriggerRegistrationError>
  Parse(base::Value::Dict&);

  static std::optional<AggregatableTriggerConfig> Create(
      mojom::SourceRegistrationTimeConfig,
      std::optional<std::string> trigger_context_id,
      AggregatableFilteringIdsMaxBytes);

  AggregatableTriggerConfig();

  AggregatableTriggerConfig(const AggregatableTriggerConfig&);
  AggregatableTriggerConfig& operator=(const AggregatableTriggerConfig&);

  AggregatableTriggerConfig(AggregatableTriggerConfig&&);
  AggregatableTriggerConfig& operator=(AggregatableTriggerConfig&&);

  ~AggregatableTriggerConfig();

  friend bool operator==(const AggregatableTriggerConfig&,
                         const AggregatableTriggerConfig&) = default;

  void Serialize(base::Value::Dict&) const;

  // Returns true when this config requires that a report be sent
  // unconditionally, i.e., if there is no report created a null report should
  // be sent.
  // https://wicg.github.io/attribution-reporting-api/#should-send-a-report-unconditionally
  bool ShouldCauseAReportToBeSentUnconditionally() const;

  mojom::SourceRegistrationTimeConfig source_registration_time_config() const {
    return source_registration_time_config_;
  }

  const std::optional<std::string>& trigger_context_id() const {
    return trigger_context_id_;
  }

  AggregatableFilteringIdsMaxBytes aggregatable_filtering_id_max_bytes() const {
    return aggregatable_filtering_id_max_bytes_;
  }

 private:
  AggregatableTriggerConfig(mojom::SourceRegistrationTimeConfig,
                            std::optional<std::string> trigger_context_id,
                            AggregatableFilteringIdsMaxBytes);

  mojom::SourceRegistrationTimeConfig source_registration_time_config_ =
      mojom::SourceRegistrationTimeConfig::kExclude;

  std::optional<std::string> trigger_context_id_;

  AggregatableFilteringIdsMaxBytes aggregatable_filtering_id_max_bytes_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_CONFIG_H_
