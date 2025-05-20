// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"

namespace base {
class DictValue;
}  // namespace base

namespace attribution_reporting {

// Conceptually a map from `uint32_t` trigger data values to `TriggerSpec`s.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerSpecs {
 public:
  using TriggerData = base::flat_set<uint32_t>;

  // Parses the top-level `trigger_data` field.
  static base::expected<TriggerSpecs, mojom::SourceRegistrationError>
  ParseTopLevelTriggerData(const base::DictValue&,
                           mojom::SourceType,
                           mojom::TriggerDataMatching);

  static std::optional<TriggerSpecs> Create(TriggerData);

  // Creates specs matching no trigger data.
  TriggerSpecs();

  // Creates specs with the default trigger data cardinality for the given
  // source type.
  explicit TriggerSpecs(mojom::SourceType);

  ~TriggerSpecs();

  TriggerSpecs(const TriggerSpecs&);
  TriggerSpecs& operator=(const TriggerSpecs&);

  TriggerSpecs(TriggerSpecs&&);
  TriggerSpecs& operator=(TriggerSpecs&&);

  base::DictValue ToJson() const;

  void Serialize(base::DictValue&) const;

  // Returns the matching trigger data, if any.
  //
  // Accepts a 64-bit integer instead of a 32-bit one for backward
  // compatibility with pre-Flex triggers that supply the full range.
  //
  // Note: `TriggerDataMatching::kModulus` can still be applied
  // even if the trigger data does not form a contiguous range starting at 0.
  // Such a combination is prohibited by `TriggerSpecs::Parse()`, but there is
  // still a well-defined meaning for it for arbitrary trigger data, so we do
  // not bother preventing it here, though we may do so in the future.
  std::optional<uint32_t> find(uint64_t trigger_data,
                               mojom::TriggerDataMatching) const;

  const TriggerData& trigger_data() const { return trigger_data_; }

  friend bool operator==(const TriggerSpecs&, const TriggerSpecs&) = default;

 private:
  explicit TriggerSpecs(TriggerData);

  TriggerData trigger_data_;
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<mojom::TriggerDataMatching, mojom::SourceRegistrationError>
ParseTriggerDataMatching(const base::DictValue&);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
void Serialize(base::DictValue&, mojom::TriggerDataMatching);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_
