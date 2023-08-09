// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_EVENT_REPORT_WINDOWS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_EVENT_REPORT_WINDOWS_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) EventReportWindows {
 public:
  // Represents the potential outcomes from checking if a trigger falls within
  // a report window.
  enum class WindowResult {
    kFallsWithin = 0,
    kPassed = 1,
    kNotStarted = 2,
    kMaxValue = kNotStarted,
  };

  static absl::optional<EventReportWindows> Create(
      base::TimeDelta start_time,
      std::vector<base::TimeDelta> end_times);

  // Creates and sets `report_window` as the last reporting window end time in
  // `end_times`, removing every existing end time greater than it.
  static absl::optional<EventReportWindows> CreateAndTruncate(
      base::TimeDelta start_time,
      std::vector<base::TimeDelta> end_times,
      base::TimeDelta report_window);

  static base::expected<EventReportWindows, mojom::SourceRegistrationError>
  FromJSON(const base::Value&);

  explicit EventReportWindows(mojo::DefaultConstruct::Tag);
  ~EventReportWindows();

  EventReportWindows(const EventReportWindows&);
  EventReportWindows& operator=(const EventReportWindows&);

  EventReportWindows(EventReportWindows&&);
  EventReportWindows& operator=(EventReportWindows&&);

  base::TimeDelta start_time() const { return start_time_; }

  const base::flat_set<base::TimeDelta>& end_times() const {
    return end_times_;
  }

  // Sets `report_window` as the last reporting window end time in `end_times_`,
  // removing every existing end time greater than it.
  // Returns whether the report window is greater than the start time, i.e.
  // returns false for invalid configurations which have no effective windows.
  bool MaybeTruncate(base::TimeDelta report_window);

  // Calculates the report time for a conversion associated with a given
  // source.
  base::Time ComputeReportTime(base::Time source_time,
                               base::Time trigger_time) const;

  base::Time ReportTimeAtWindow(base::Time source_time, int window_index) const;

  WindowResult FallsWithin(base::TimeDelta trigger_moment) const;

  base::Value::Dict ToJson() const;

 private:
  EventReportWindows(base::TimeDelta start_time,
                     base::flat_set<base::TimeDelta> end_times);

  base::TimeDelta start_time_;
  base::flat_set<base::TimeDelta> end_times_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_EVENT_REPORT_WINDOWS_H_
