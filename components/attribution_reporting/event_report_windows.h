// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_EVENT_REPORT_WINDOWS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_EVENT_REPORT_WINDOWS_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"

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

  static std::optional<EventReportWindows> Create(
      base::TimeDelta start_time,
      std::vector<base::TimeDelta> end_times);

  // Uses default windows based on the source type, but truncated at
  // `report_window`.
  static std::optional<EventReportWindows> FromDefaults(
      base::TimeDelta report_window,
      mojom::SourceType);

  static base::expected<EventReportWindows, mojom::SourceRegistrationError>
  FromJSON(const base::Value::Dict& registration,
           base::TimeDelta expiry,
           mojom::SourceType);

  static base::expected<EventReportWindows, mojom::SourceRegistrationError>
  ParseWindows(const base::Value::Dict&,
               base::TimeDelta expiry,
               const EventReportWindows& default_if_absent);

  // Creates a single report window at `kMaxSourceExpiry`.
  EventReportWindows();

  ~EventReportWindows();

  EventReportWindows(const EventReportWindows&);
  EventReportWindows& operator=(const EventReportWindows&);

  EventReportWindows(EventReportWindows&&);
  EventReportWindows& operator=(EventReportWindows&&);

  base::TimeDelta start_time() const { return start_time_; }

  const base::flat_set<base::TimeDelta>& end_times() const {
    return end_times_;
  }

  bool IsValidForExpiry(base::TimeDelta expiry) const;

  // Calculates the report time for a conversion associated with a given
  // source.
  base::Time ComputeReportTime(base::Time source_time,
                               base::Time trigger_time) const;

  base::Time ReportTimeAtWindow(base::Time source_time, int window_index) const;

  base::Time StartTimeAtWindow(base::Time source_time, int window_index) const;

  WindowResult FallsWithin(base::TimeDelta trigger_moment) const;

  void Serialize(base::Value::Dict& dict) const;

  friend bool operator==(const EventReportWindows&,
                         const EventReportWindows&) = default;

 private:
  EventReportWindows(base::TimeDelta start_time,
                     base::flat_set<base::TimeDelta> end_times);

  EventReportWindows(base::TimeDelta report_window, mojom::SourceType);

  static base::expected<EventReportWindows, mojom::SourceRegistrationError>
  ParseWindowsJSON(const base::Value&, base::TimeDelta expiry);

  base::TimeDelta start_time_;
  base::flat_set<base::TimeDelta> end_times_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_EVENT_REPORT_WINDOWS_H_
