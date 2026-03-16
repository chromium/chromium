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

  void TriggerContextUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus context_upload_status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type) {
    for (auto& observer : observers_) {
      observer.OnContextUploadStatusChanged(context_token, mime_type,
                                            context_upload_status, error_type);
    }
  }

  void AddObserver(ContextUploadStatusObserver* obs) override {
    observers_.AddObserver(obs);
  }
  void RemoveObserver(ContextUploadStatusObserver* obs) override {
    observers_.RemoveObserver(obs);
  }

 private:
  base::ObserverList<ContextUploadStatusObserver> observers_;
};

class MockContextualSearchMetricsRecorder
    : public ContextualSearchMetricsRecorder {
 public:
  MockContextualSearchMetricsRecorder()
      : ContextualSearchMetricsRecorder(ContextualSearchSource::kUnknown) {}
  ~MockContextualSearchMetricsRecorder() override = default;

  MOCK_METHOD(void,
              OnContextUploadStatusChanged,
              (lens::MimeType,
               contextual_search::ContextUploadStatus,
               const std::optional<contextual_search::ContextUploadErrorType>&),
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

TEST_F(ContextualSearchSessionEntryTest, ForwardsContextUploadStatusChanged) {
  base::UnguessableToken context_token = base::UnguessableToken::Create();
  lens::MimeType mime_type = lens::MimeType::kPdf;
  contextual_search::ContextUploadStatus status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  std::optional<contextual_search::ContextUploadErrorType> error_type =
      std::nullopt;

  EXPECT_CALL(*metrics_recorder_ptr_,
              OnContextUploadStatusChanged(mime_type, status, error_type))
      .Times(1);

  controller_ptr_->TriggerContextUploadStatusChanged(context_token, mime_type,
                                                     status, error_type);
}

}  // namespace contextual_search
