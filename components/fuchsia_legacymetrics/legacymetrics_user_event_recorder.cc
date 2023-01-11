// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_legacymetrics/legacymetrics_user_event_recorder.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"

namespace fuchsia_legacymetrics {

constexpr size_t LegacyMetricsUserActionRecorder::kMaxEventCount;

LegacyMetricsUserActionRecorder::LegacyMetricsUserActionRecorder()
    : on_event_callback_(
          base::BindRepeating(&LegacyMetricsUserActionRecorder::OnUserAction,
                              base::Unretained(this))) {
  base::AddActionCallback(on_event_callback_);
}

LegacyMetricsUserActionRecorder::~LegacyMetricsUserActionRecorder() {
  base::RemoveActionCallback(on_event_callback_);
}

bool LegacyMetricsUserActionRecorder::HasEvents() const {
  return !events_.empty();
}

std::vector<fuchsia::legacymetrics::UserActionEvent>
LegacyMetricsUserActionRecorder::TakeEvents() {
  return std::move(events_);
}

void LegacyMetricsUserActionRecorder::OnUserAction(const std::string& action,
                                                   base::TimeTicks time) {
  if (events_.size() >= kMaxEventCount)
    return;

  fuchsia::legacymetrics::UserActionEvent fidl_event;
  fidl_event.set_name(action);
  fidl_event.set_time(time.ToZxTime());
  events_.push_back(std::move(fidl_event));
}

}  // namespace fuchsia_legacymetrics
