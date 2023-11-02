// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_action_recorder.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"

namespace chromecast {
namespace {

constexpr size_t kActionLimit = 1000000;

}  // namespace

CastRuntimeActionRecorder::CastRuntimeActionRecorder()
    : on_action_callback_(
          base::BindRepeating(&CastRuntimeActionRecorder::OnAction,
                              base::Unretained(this))) {
  DCHECK(base::GetRecordActionTaskRunner());
  base::AddActionCallback(on_action_callback_);
}

CastRuntimeActionRecorder::~CastRuntimeActionRecorder() {
  base::RemoveActionCallback(on_action_callback_);
}

std::vector<cast::metrics::UserActionEvent>
CastRuntimeActionRecorder::TakeEvents() {
  return std::move(events_);
}

void CastRuntimeActionRecorder::OnAction(const std::string& name,
                                         base::TimeTicks time) {
  if (events_.size() >= kActionLimit) {
    static bool logged_once = false;
    if (!logged_once) {
      LOG(ERROR) << "Too many actions, dropping...";
      logged_once = true;
    }
    return;
  }
  cast::metrics::UserActionEvent event;
  event.set_name(name);
  event.set_time((time - base::TimeTicks()).InMicroseconds());
  events_.push_back(std::move(event));
}

}  // namespace chromecast
