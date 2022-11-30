// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_USER_EVENT_RECORDER_H_
#define COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_USER_EVENT_RECORDER_H_

#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <string>
#include <vector>

#include "base/metrics/user_metrics.h"

namespace fuchsia_legacymetrics {

// Captures and stores user action events, and converts them to
// fuchsia.legacymetrics equivalent.
class LegacyMetricsUserActionRecorder {
 public:
  // Maximum number of Events to store locally before dropping new ones.
  static constexpr size_t kMaxEventCount = 5000;

  LegacyMetricsUserActionRecorder();
  ~LegacyMetricsUserActionRecorder();

  LegacyMetricsUserActionRecorder(const LegacyMetricsUserActionRecorder&) =
      delete;
  LegacyMetricsUserActionRecorder& operator=(
      const LegacyMetricsUserActionRecorder&) = delete;

  bool HasEvents() const;
  std::vector<fuchsia::legacymetrics::UserActionEvent> TakeEvents();

 private:
  // base::ActionCallback implementation.
  void OnUserAction(const std::string& action, base::TimeTicks time);

  std::vector<fuchsia::legacymetrics::UserActionEvent> events_;
  const base::ActionCallback on_event_callback_;
};

}  // namespace fuchsia_legacymetrics

#endif  // COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_USER_EVENT_RECORDER_H_
