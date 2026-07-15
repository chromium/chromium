// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_RECORDER_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_RECORDER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class Profile;

namespace ui {
class Event;
}

class WaapUIMetricsService;

// `WaapUIMetricsRecorder` is responsible for tracking state and timings
// related to user interactions within the WaaP UIs, which for now is limited to
// the TopChrome ReloadButton UI.
// Each ReloadButton instance should own an instance of this recorder.
class WaapUIMetricsRecorder {
 public:
  // Input type to activate the ReloadButton for use in this recorder.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ReloadButtonInputType)
  enum class ReloadButtonInputType {
    kMouseRelease = 0,
    kKeyPress = 1,
    kMaxValue = kKeyPress
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:ReloadButtonInputType)

  // Mode of the ReloadButton for use in this recorder.
  enum class ReloadButtonMode { kReload = 0, kStop = 1, kMaxValue = kStop };

  explicit WaapUIMetricsRecorder(Profile* profile);

  WaapUIMetricsRecorder(const WaapUIMetricsRecorder&) = delete;
  WaapUIMetricsRecorder& operator=(const WaapUIMetricsRecorder&) = delete;

  ~WaapUIMetricsRecorder();

  // Called at the start of ReloadButton::ButtonPressed.
  void OnButtonPressedStart(const ui::Event& event);

  // Called after the Reload command has been executed.
  void DidExecuteReloadCommand(base::TimeTicks time);

 private:
  // Information about the last input event that triggered ButtonPressed.
  struct LastInputInfo {
    base::TimeTicks time;
    ReloadButtonInputType type;
  };

  // This may be null if profile is null, e.g. in tests, or if the feature is
  // disabled. Its lifetime is managed by the `WaapUIMetricsServiceFactory` and
  // is guaranteed to outlive this object as long as none null.
  // Not owned.
  const raw_ptr<WaapUIMetricsService> waap_service_;

  // State related to the last ButtonPressed input event.
  std::optional<LastInputInfo> last_input_info_;
};

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_RECORDER_H_
