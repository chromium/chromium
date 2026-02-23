// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_CONTROLLER_H_

#include <string>
#include <string_view>

namespace record_replay {

// Interface for the controller of the Save Recording bubble.
class SaveRecordingBubbleController {
 public:
  virtual ~SaveRecordingBubbleController() = default;

  // Called when the user clicks the "Save" button in the bubble.
  // |name| is the name entered by the user.
  virtual void OnSave(std::u16string_view name) = 0;

  // Called when the user clicks the "Cancel" button in the bubble.
  virtual void OnCancel() = 0;

  // Called when the bubble is closed (either by user action or
  // programmatically).
  virtual void OnBubbleClosed() = 0;

  // Returns the URL of the recording's starting point.
  virtual std::string_view GetUrl() const = 0;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_UI_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_CONTROLLER_H_
