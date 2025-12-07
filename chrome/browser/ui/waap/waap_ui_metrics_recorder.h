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

  // Called when the mouse enters the ReloadButton.
  void OnMouseEntered(base::TimeTicks time);
  // Called when the mouse exits the ReloadButton.
  void OnMouseExited(base::TimeTicks time);
  // Called when a mouse button is pressed on the ReloadButton.
  void OnMousePressed(base::TimeTicks time);
  // Called when a mouse button is released on the ReloadButton.
  void OnMouseReleased(base::TimeTicks time);

  // Called at the start of ReloadButton::ButtonPressed.
  void OnButtonPressedStart(const ui::Event& event,
                            ReloadButtonMode current_mode);
  // Called after the Stop command has been executed.
  void DidExecuteStopCommand(base::TimeTicks time);
  // Called after the Reload command has been executed.
  void DidExecuteReloadCommand(base::TimeTicks time);
  // Called when the visible mode of the ReloadButton possibly changes.
  void OnChangeVisibleMode(ReloadButtonMode current_mode,
                           ReloadButtonMode intended_mode,
                           base::TimeTicks time);
  // Called when a viz frame that contains the ReloadButton is successfully
  // painted on the screen.
  //
  // `visible_mode` is the mode of the ReloadButton at the time of it requesting
  // to paint.
  // `button_state` is the state of the ReloadButton at the time of it
  // requesting to paint.
  // `now` is the time at which the frame was presented.
  void OnPaintFramePresented(ReloadButtonMode visible_mode,
                             int button_state,
                             base::TimeTicks now);

 private:
  // Information about the last input event that triggered ButtonPressed.
  struct LastInputInfo {
    base::TimeTicks time;
    ReloadButtonInputType type;
    ReloadButtonMode mode_at_input;
  };

  // Information about a pending mode change visual update.
  struct PendingModeChange {
    base::TimeTicks start_time;
    ReloadButtonMode target_mode = ReloadButtonMode::kReload;
  };

  // This may be null if profile is null, e.g. in tests, or if the feature is
  // disabled. Its lifetime is managed by the `WaapUIMetricsServiceFactory` and
  // is guaranteed to outlive this object as long as none null.
  // Not owned.
  const raw_ptr<WaapUIMetricsService> waap_service_;

  // The timestamp of the last mouse enter event.
  base::TimeTicks mouse_entered_time_;
  // The timestamp of the last mouse press event.
  // Both left and right clicks are considered.
  base::TimeTicks mouse_pressed_time_;

  // State related to the last ButtonPressed input event.
  std::optional<const LastInputInfo> last_input_info_ = std::nullopt;
  // State related to a pending mode change.
  std::optional<const PendingModeChange> pending_mode_change_ = std::nullopt;
};

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_RECORDER_H_
