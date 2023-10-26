// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_

#include <stdint.h>

#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerSpec {
 public:
  explicit TriggerSpec(EventReportWindows);

  ~TriggerSpec();

  TriggerSpec(const TriggerSpec&);
  TriggerSpec& operator=(const TriggerSpec&);

  TriggerSpec(TriggerSpec&&);
  TriggerSpec& operator=(TriggerSpec&&);

  const EventReportWindows& event_report_windows() const {
    return event_report_windows_;
  }

  base::Value::Dict ToJson() const;

 private:
  EventReportWindows event_report_windows_;
};

// Conceptually a map from `uint32_t` trigger data values to `TriggerSpec`s.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerSpecs {
 public:
  using TriggerDataIndices = base::flat_map<uint32_t, uint8_t>;
  using value_type = std::pair<uint32_t, const TriggerSpec&>;

  static base::expected<TriggerSpecs, mojom::SourceRegistrationError> Parse(
      const base::Value::Dict&,
      mojom::SourceType,
      base::TimeDelta expiry,
      EventReportWindows default_report_windows,
      mojom::TriggerDataMatching);

  // Creates specs with the default trigger data cardinality for the given
  // source type.
  static TriggerSpecs Default(mojom::SourceType, EventReportWindows);

  static TriggerSpecs CreateForTesting(TriggerDataIndices,
                                       std::vector<TriggerSpec>);

  // Creates specs matching no trigger data.
  TriggerSpecs();

  ~TriggerSpecs();

  TriggerSpecs(const TriggerSpecs&);
  TriggerSpecs& operator=(const TriggerSpecs&);

  TriggerSpecs(TriggerSpecs&&);
  TriggerSpecs& operator=(TriggerSpecs&&);

  bool empty() const { return trigger_data_indices_.empty(); }

  size_t size() const { return trigger_data_indices_.size(); }

  // TODO(apaseltiner): Add a `find(uint32_t)` method that performs an optimized
  // lookup for a given trigger data value and use it in
  // `content::AttributionStorageSql`.

  void Serialize(base::Value::Dict&) const;

  class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = TriggerSpecs::value_type;
    // This is *not* a reference type because the values are produced on demand.
    using reference = value_type;
    using pointer = void;

    reference operator*() const {
      return value_type(it_->first, specs_->specs_[it_->second]);
    }

    Iterator& operator++() {
      it_++;
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    // Returns the index of the current trigger_data in the sorted list of all
    // trigger_data.
    uint8_t index() const {
      return base::checked_cast<uint8_t>(
          std::distance(specs_->trigger_data_indices_.cbegin(), it_));
    }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      return a.it_ == b.it_;
    }

    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return a.it_ != b.it_;
    }

   private:
    friend TriggerSpecs;

    Iterator(const TriggerSpecs&, TriggerDataIndices::const_iterator);

    const raw_ref<const TriggerSpecs> specs_;
    TriggerDataIndices::const_iterator it_;
  };

  using const_iterator = Iterator;

  const_iterator begin() const {
    return Iterator(*this, trigger_data_indices_.cbegin());
  }

  const_iterator end() const {
    return Iterator(*this, trigger_data_indices_.cend());
  }

 private:
  friend Iterator;

  TriggerSpecs(TriggerDataIndices, std::vector<TriggerSpec>);

  // These two fields effectively act as a compressed `base::flat_map<uint32_t,
  // scoped_refptr<TriggerSpec>>`, optimized for the fact that there are at most
  // 32 specs, meaning that their indices fit into a `uint8_t`. Storing the
  // `TriggerSpec`s in a separate vector also makes it easier to regroup the
  // individual keys by spec, because those specs will have the same index in
  // `specs_`. Without that, serialization, e.g. in the
  // `TriggerSpecs::Serialize()` method and the forthcoming analogue for
  // persistence on disk, would have to perform an additional group-by operation
  // using `scoped_refptr` address.
  TriggerDataIndices trigger_data_indices_;
  std::vector<TriggerSpec> specs_;
};

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
