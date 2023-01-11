// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_ACTION_RECORDER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_ACTION_RECORDER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.pb.h"

namespace chromecast {

// This class records user action events.  It begins recording via a callback at
// construction time.  The current set of events can be taken with TakeEvents to
// send to Cast Core.
class CastRuntimeActionRecorder {
 public:
  CastRuntimeActionRecorder();
  ~CastRuntimeActionRecorder();

  CastRuntimeActionRecorder(const CastRuntimeActionRecorder&) = delete;
  CastRuntimeActionRecorder& operator=(const CastRuntimeActionRecorder&) =
      delete;

  std::vector<cast::metrics::UserActionEvent> TakeEvents();

 private:
  void OnAction(const std::string& name, base::TimeTicks time);

  base::RepeatingCallback<void(const std::string&, base::TimeTicks)>
      on_action_callback_;
  std::vector<cast::metrics::UserActionEvent> events_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_ACTION_RECORDER_H_
