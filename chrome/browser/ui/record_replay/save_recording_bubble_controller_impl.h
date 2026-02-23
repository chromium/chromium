// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_CONTROLLER_IMPL_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "chrome/browser/ui/record_replay/save_recording_bubble_controller.h"

namespace record_replay {

class RecordingDataManager;

// Implementation of the SaveRecordingBubbleController.
// Handles the logic for saving or discarding a recording.
class SaveRecordingBubbleControllerImpl : public SaveRecordingBubbleController {
 public:
  SaveRecordingBubbleControllerImpl(
      Recording recording,
      RecordingDataManager* recording_data_manager,
      base::OnceCallback<void(std::string_view)> show_toast_callback,
      base::OnceClosure on_close_closure);
  ~SaveRecordingBubbleControllerImpl() override;

  // SaveRecordingBubbleController:
  void OnSave(std::u16string_view name) override;
  void OnCancel() override;
  void OnBubbleClosed() override;
  std::string_view GetUrl() const override;

 private:
  Recording recording_;
  const raw_ref<RecordingDataManager> recording_data_manager_;
  base::OnceCallback<void(std::string_view)> show_toast_callback_;
  base::OnceClosure on_close_closure_;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_UI_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_CONTROLLER_IMPL_H_
