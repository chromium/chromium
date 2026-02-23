// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/record_replay/save_recording_bubble_controller_impl.h"

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/record_replay/recording_data_manager.h"

namespace record_replay {

SaveRecordingBubbleControllerImpl::SaveRecordingBubbleControllerImpl(
    Recording recording,
    RecordingDataManager* recording_data_manager,
    base::OnceCallback<void(std::string_view)> show_toast_callback,
    base::OnceClosure on_close_closure)
    : recording_(std::move(recording)),
      recording_data_manager_(*recording_data_manager),
      show_toast_callback_(std::move(show_toast_callback)),
      on_close_closure_(std::move(on_close_closure)) {
  CHECK(recording_data_manager);
}

SaveRecordingBubbleControllerImpl::~SaveRecordingBubbleControllerImpl() {
  if (on_close_closure_) {
    std::move(on_close_closure_).Run();
  }
}

void SaveRecordingBubbleControllerImpl::OnSave(std::u16string_view name) {
  recording_.set_name(base::UTF16ToUTF8(name));
  recording_data_manager_->AddRecording(std::move(recording_));
  if (show_toast_callback_) {
    std::move(show_toast_callback_).Run("Recording saved");
  }
}

void SaveRecordingBubbleControllerImpl::OnCancel() {
}

void SaveRecordingBubbleControllerImpl::OnBubbleClosed() {
  // Self-destruction is handled by the owner of this controller.
  // The owner (likely RecordReplayPageActionController) should clean up
  // when the bubble closes.
  CHECK(on_close_closure_);
  std::move(on_close_closure_).Run();
}

std::string_view SaveRecordingBubbleControllerImpl::GetUrl() const {
  return recording_.url();
}

}  // namespace record_replay
