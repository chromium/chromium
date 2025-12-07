// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_entry.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/test/mock_callback.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_search {

class TestContextualSearchContextController
    : public MockContextualSearchContextController {
 public:
  TestContextualSearchContextController() = default;
  ~TestContextualSearchContextController() override = default;

  void TriggerFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      contextual_search::FileUploadStatus file_upload_status,
      const std::optional<contextual_search::FileUploadErrorType>& error_type) {
    for (auto& observer : observers_) {
      observer.OnFileUploadStatusChanged(file_token, mime_type,
                                         file_upload_status, error_type);
    }
  }

  void AddObserver(FileUploadStatusObserver* obs) override {
    observers_.AddObserver(obs);
  }
  void RemoveObserver(FileUploadStatusObserver* obs) override {
    observers_.RemoveObserver(obs);
  }

 private:
  base::ObserverList<FileUploadStatusObserver> observers_;
};

class MockContextualSearchMetricsRecorder
    : public ContextualSearchMetricsRecorder {
 public:
  MockContextualSearchMetricsRecorder()
      : ContextualSearchMetricsRecorder(ContextualSearchSource::kUnknown) {}
  ~MockContextualSearchMetricsRecorder() override = default;

  MOCK_METHOD(void,
              OnFileUploadStatusChanged,
              (lens::MimeType,
               contextual_search::FileUploadStatus,
               const std::optional<contextual_search::FileUploadErrorType>&),
              (override));
};

class ContextualSearchSessionEntryTest : public testing::Test {
 public:
  ContextualSearchSessionEntryTest() = default;
  ~ContextualSearchSessionEntryTest() override = default;

  void SetUp() override {
    auto controller = std::make_unique<TestContextualSearchContextController>();
    controller_ptr_ = controller.get();
    auto metrics_recorder =
        std::make_unique<MockContextualSearchMetricsRecorder>();
    metrics_recorder_ptr_ = metrics_recorder.get();
    session_entry_ = base::WrapUnique(new ContextualSearchSessionEntry(
        std::move(controller), std::move(metrics_recorder)));
  }

  void TearDown() override {
    controller_ptr_ = nullptr;
    metrics_recorder_ptr_ = nullptr;
    session_entry_.reset();
    testing::Test::TearDown();
  }

 protected:
  raw_ptr<TestContextualSearchContextController> controller_ptr_;
  raw_ptr<MockContextualSearchMetricsRecorder> metrics_recorder_ptr_;
  std::unique_ptr<ContextualSearchSessionEntry> session_entry_;
};

TEST_F(ContextualSearchSessionEntryTest, ForwardsFileUploadStatusChanged) {
  base::UnguessableToken file_token = base::UnguessableToken::Create();
  lens::MimeType mime_type = lens::MimeType::kPdf;
  contextual_search::FileUploadStatus status =
      contextual_search::FileUploadStatus::kUploadSuccessful;
  std::optional<contextual_search::FileUploadErrorType> error_type = std::nullopt;

  EXPECT_CALL(*metrics_recorder_ptr_,
              OnFileUploadStatusChanged(mime_type, status, error_type))
      .Times(1);

  controller_ptr_->TriggerFileUploadStatusChanged(file_token, mime_type, status,
                                                  error_type);
}

}  // namespace contextual_search
