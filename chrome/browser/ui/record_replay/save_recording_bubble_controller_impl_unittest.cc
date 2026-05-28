// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/record_replay/save_recording_bubble_controller_impl.h"

#include "base/test/mock_callback.h"
#include "components/record_replay/core/common/test_support/mock_task_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

class SaveRecordingBubbleControllerImplTest : public testing::Test {};

TEST_F(SaveRecordingBubbleControllerImplTest, OnSave_SavesRecording) {
  Recording recording;
  recording.set_url("http://example.com");

  MockTaskStore mock_task_store;
  base::MockCallback<base::OnceCallback<void(std::string_view)>> show_toast;
  base::MockCallback<base::OnceClosure> on_close;

  EXPECT_CALL(mock_task_store, AddRecording(testing::_, testing::_))
      .WillOnce([](Recording r, base::OnceCallback<void(int64_t)> callback) {
        EXPECT_EQ(r.url(), "http://example.com");
        EXPECT_EQ(r.name(), "Test Name");
        std::move(callback).Run(1);
      });
  EXPECT_CALL(show_toast, Run(testing::Eq("Recording saved")));

  auto controller = std::make_unique<SaveRecordingBubbleControllerImpl>(
      std::move(recording), &mock_task_store, show_toast.Get(), on_close.Get());

  controller->OnSave(u"Test Name");
  controller->OnBubbleClosed();
}

TEST_F(SaveRecordingBubbleControllerImplTest, OnCancel_DoesNotSave) {
  Recording recording;
  MockTaskStore mock_task_store;
  base::MockCallback<base::OnceCallback<void(std::string_view)>> show_toast;
  base::MockCallback<base::OnceClosure> on_close;

  EXPECT_CALL(mock_task_store, AddRecording(testing::_, testing::_)).Times(0);
  EXPECT_CALL(show_toast, Run(testing::_)).Times(0);

  auto controller = std::make_unique<SaveRecordingBubbleControllerImpl>(
      std::move(recording), &mock_task_store, show_toast.Get(), on_close.Get());

  controller->OnCancel();
  controller->OnBubbleClosed();
}

}  // namespace record_replay
