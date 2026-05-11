// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/record_replay/save_recording_bubble_controller_impl.h"

#include "base/test/mock_callback.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/recording_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

class MockRecordingDataManager : public RecordingDataManager {
 public:
  MOCK_METHOD(void,
              AddRecording,
              (Recording recording, base::OnceCallback<void(int64_t)> callback),
              (override));
  MOCK_METHOD(void,
              GetRecordingsByUrl,
              (std::string url,
               base::OnceCallback<void(std::vector<Recording>)> callback),
              (override));
  MOCK_METHOD(void,
              SaveTaskDefinition,
              (std::optional<int64_t> task_definition_id,
               TaskDefinition task_definition,
               std::string target_url,
               std::optional<int64_t> recording_id,
               base::OnceClosure callback),
              (override));
  MOCK_METHOD(
      void,
      GetTaskDefinition,
      (int64_t task_definition_id,
       base::OnceCallback<void(std::optional<TaskDefinition>)> callback),
      (override));
  MOCK_METHOD(
      void,
      GetTaskDefinitionsByUrl,
      (std::string url,
       base::OnceCallback<void(std::vector<std::pair<int64_t, TaskDefinition>>)>
           callback),
      (override));
  MOCK_METHOD(void,
              SaveTaskData,
              (int64_t task_definition_id,
               TaskData data,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              GetTaskData,
              (int64_t task_definition_id,
               base::OnceCallback<void(std::optional<TaskData>)> callback),
              (override));
  MOCK_METHOD(void,
              DeleteTaskData,
              (int64_t task_definition_id,
               base::OnceCallback<void(bool)> callback),
              (override));
};

class SaveRecordingBubbleControllerImplTest : public testing::Test {};

TEST_F(SaveRecordingBubbleControllerImplTest, OnSave_SavesRecording) {
  Recording recording;
  recording.set_url("http://example.com");

  MockRecordingDataManager mock_manager;
  base::MockCallback<base::OnceCallback<void(std::string_view)>> show_toast;
  base::MockCallback<base::OnceClosure> on_close;

  EXPECT_CALL(mock_manager, AddRecording(testing::_, testing::_))
      .WillOnce([](Recording r, base::OnceCallback<void(int64_t)> callback) {
        EXPECT_EQ(r.url(), "http://example.com");
        EXPECT_EQ(r.name(), "Test Name");
        std::move(callback).Run(1);
      });
  EXPECT_CALL(show_toast, Run(testing::Eq("Recording saved")));

  auto controller = std::make_unique<SaveRecordingBubbleControllerImpl>(
      std::move(recording), &mock_manager, show_toast.Get(), on_close.Get());

  controller->OnSave(u"Test Name");
  controller->OnBubbleClosed();
}

TEST_F(SaveRecordingBubbleControllerImplTest, OnCancel_DoesNotSave) {
  Recording recording;
  MockRecordingDataManager mock_manager;
  base::MockCallback<base::OnceCallback<void(std::string_view)>> show_toast;
  base::MockCallback<base::OnceClosure> on_close;

  EXPECT_CALL(mock_manager, AddRecording(testing::_, testing::_)).Times(0);
  EXPECT_CALL(show_toast, Run(testing::_)).Times(0);

  auto controller = std::make_unique<SaveRecordingBubbleControllerImpl>(
      std::move(recording), &mock_manager, show_toast.Get(), on_close.Get());

  controller->OnCancel();
  controller->OnBubbleClosed();
}

}  // namespace record_replay
