// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_handle.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace contextual_search {

class ContextualSearchSessionHandleTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_controller =
        std::make_unique<MockContextualSearchContextController>();
    mock_controller_ptr_ = mock_controller.get();

    auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
        ContextualSearchSource::kUnknown);

    service_ = std::make_unique<ContextualSearchService>(
        nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "");

    handle_ = service_->CreateSessionForTesting(std::move(mock_controller),
                                                std::move(metrics_recorder));

    ContextualSearchService::RegisterProfilePrefs(prefs_.registry());
    handle_->CheckSearchContentSharingSettings(&prefs_);
  }

  TestingPrefServiceSimple prefs_;
  std::unique_ptr<ContextualSearchService> service_;
  std::unique_ptr<ContextualSearchSessionHandle> handle_;
  raw_ptr<MockContextualSearchContextController> mock_controller_ptr_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ContextualSearchSessionHandleTest,
       StartFileContextUploadFlow_FallbackToUnknown) {
  // Ensure the feature is disabled.
  feature_list_.InitAndDisableFeature(
      lens::features::kLensSendRawFileMediaTypes);

  base::UnguessableToken token = handle_->CreateContextToken();

  // Expect StartFileUploadFlow to be called.
  EXPECT_CALL(*mock_controller_ptr_, StartFileUploadFlow(token, _, _))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> input_data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(input_data->primary_content_type, lens::MimeType::kUnknown);
      });

  mojo_base::BigBuffer buffer;
  handle_->StartFileContextUploadFlow(token, "test.txt", "text/plain",
                                      std::move(buffer), std::nullopt);
}

}  // namespace contextual_search
