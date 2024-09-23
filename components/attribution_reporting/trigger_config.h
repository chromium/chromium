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
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerSpec {
 public:
  TriggerSpec();

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

  friend bool operator==(const TriggerSpec&, const TriggerSpec&) = default;

 private:
  EventReportWindows event_report_windows_;
};

// Conceptually a map from `uint32_t` trigger data values to `TriggerSpec`s.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerSpecs {
 public:
  using TriggerDataIndices = base::flat_map<uint32_t, uint8_t>;
  using value_type = std::pair<uint32_t, const TriggerSpec&>;

  // TODO: Merge `ParseTopLevelTriggerData()` into this function and rename it
  // to `Parse()`.
  static base::expected<TriggerSpecs, mojom::SourceRegistrationError>
  ParseFullFlexForTesting(const base::Value::Dict&,
                          mojom::SourceType,
                          base::TimeDelta expiry,
                          EventReportWindows default_report_windows,
                          mojom::TriggerDataMatching);

  // Parses the top-level `trigger_data` field. The resulting value is either
  // `empty()` or `SingleSharedSpec()`.
  static base::expected<TriggerSpecs, mojom::SourceRegistrationError>
  ParseTopLevelTriggerData(const base::Value::Dict&,
                           mojom::SourceType,
                           EventReportWindows default_report_windows,
                           mojom::TriggerDataMatching);

  static std::optional<TriggerSpecs> Create(TriggerDataIndices,
                                            std::vector<TriggerSpec>,
                                            MaxEventLevelReports);

  // Creates specs matching no trigger data.
  TriggerSpecs();

  // Creates specs with the default trigger data cardinality for the given
  // source type.
  TriggerSpecs(mojom::SourceType, EventReportWindows, MaxEventLevelReports);

  ~TriggerSpecs();

  TriggerSpecs(const TriggerSpecs&);
  TriggerSpecs& operator=(const TriggerSpecs&);

  TriggerSpecs(TriggerSpecs&&);
  TriggerSpecs& operator=(TriggerSpecs&&);

  bool empty() const { return trigger_data_indices_.empty(); }

  size_t size() const { return trigger_data_indices_.size(); }

  // Will return nullptr if there is not a single shared spec.
  const TriggerSpec* SingleSharedSpec() const;

  base::Value::List ToJson() const;

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
      return value_type(it_->first, specs_->specs()[it_->second]);
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
          std::distance(specs_->trigger_data_indices().cbegin(), it_));
    }

    explicit operator bool() const {
      return it_ != specs_->trigger_data_indices().end();
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

  // Returns the matching trigger spec and its associated trigger data, if any.
  // Returns `TriggerSpecs::end()` if there is no match.
  //
  // Accepts a 64-bit integer instead of a 32-bit one for backward
  // compatibility with pre-Flex triggers that supply the full range.
  //
  // Note: `TriggerDataMatching::kModulus` can still be applied
  // even if the trigger data does not form a contiguous range starting at 0.
  // Such a combination is prohibited by `TriggerSpecs::Parse()`, but there is
  // still a well-defined meaning for it for arbitrary trigger data, so we do
  // not bother preventing it here, though we may do so in the future.
  const_iterator find(uint64_t trigger_data, mojom::TriggerDataMatching) const;

  const TriggerDataIndices& trigger_data_indices() const {
    return trigger_data_indices_;
  }

  const std::vector<TriggerSpec>& specs() const { return specs_; }

  MaxEventLevelReports max_event_level_reports() const {
    return max_event_level_reports_;
  }

  void SetMaxEventLevelReportsForTesting(
      MaxEventLevelReports max_event_level_reports) {
    max_event_level_reports_ = max_event_level_reports;
  }

  friend bool operator==(const TriggerSpecs&, const TriggerSpecs&) = default;

 private:
  TriggerSpecs(TriggerDataIndices,
               std::vector<TriggerSpec>,
               MaxEventLevelReports);

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

  MaxEventLevelReports max_event_level_reports_;
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<mojom::TriggerDataMatching, mojom::SourceRegistrationError>
ParseTriggerDataMatching(const base::Value::Dict&);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
void Serialize(base::Value::Dict&, mojom::TriggerDataMatching);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_CONFIG_H_
