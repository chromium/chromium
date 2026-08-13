// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_handle.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace contextual_search {

class MockTabValidator : public ContextualSearchSessionHandle::TabValidator {
 public:
  MOCK_METHOD(bool, IsTabValidAndPointingToUrl, (const FileInfo&), (override));
  MOCK_METHOD(
      bool,
      AreUrlsEquivalent,
      (const GURL&, const std::string&, const GURL&, const std::string&),
      (override));
};

class ContextualSearchSessionHandleTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_controller =
        std::make_unique<MockContextualSearchContextController>();
    mock_controller_ptr_ = mock_controller.get();

    auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
        ContextualSearchSource::kUnknown);

    service_ = std::make_unique<ContextualSearchService>(
        nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
        /*tab_validator=*/nullptr);

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
        EXPECT_EQ(input_data->upload_type,
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
      });

  mojo_base::BigBuffer buffer;
  handle_->StartFileContextUploadFlow(token, "test.txt", "text/plain",
                                      std::move(buffer), std::nullopt);
}

TEST_F(ContextualSearchSessionHandleTest,
       StartFileContextUploadFlow_SvgFallbackToUnknownWhenRawFilesEnabled) {
  // Ensure the feature is enabled.
  feature_list_.InitAndEnableFeature(
      lens::features::kLensSendRawFileMediaTypes);

  base::UnguessableToken token = handle_->CreateContextToken();

  // Expect `StartFileUploadFlow` to be called with `kUnknown` for SVG.
  EXPECT_CALL(*mock_controller_ptr_, StartFileUploadFlow(token, _, _))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> input_data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(input_data->primary_content_type, lens::MimeType::kUnknown);
      });

  mojo_base::BigBuffer buffer;
  handle_->StartFileContextUploadFlow(token, "test.svg", "image/svg+xml",
                                      std::move(buffer), std::nullopt);
}

TEST_F(ContextualSearchSessionHandleTest,
       StartDriveContextUploadFlow_ValidToken) {
  base::UnguessableToken token = handle_->CreateContextToken();
  std::string test_drive_id = "test_drive_id";
  std::string test_resource_key = "test_resource_key";
  std::string test_mime_type = "application/vnd.google-apps.document";

  EXPECT_CALL(*mock_controller_ptr_, StartFileUploadFlow(token, _, _))
      .WillOnce([&](const base::UnguessableToken& file_token,
                    std::unique_ptr<lens::ContextualInputData> input_data,
                    std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(input_data->primary_content_type, lens::MimeType::kUnknown);
        EXPECT_EQ(input_data->drive_id, test_drive_id);
        EXPECT_EQ(input_data->resource_key, test_resource_key);
        EXPECT_EQ(input_data->mime_type_string, test_mime_type);
        EXPECT_EQ(input_data->upload_type,
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
      });

  ContextualSearchSessionHandle::DriveUploadParams params;
  params.drive_id = test_drive_id;
  params.resource_key = test_resource_key;
  params.mime_type = test_mime_type;
  params.file_name = "test.doc";
  handle_->StartDriveContextUploadFlow(token, params);
}

TEST_F(ContextualSearchSessionHandleTest,
       StartDriveContextUploadFlow_InvalidToken) {
  base::UnguessableToken token = base::UnguessableToken::Create();

  EXPECT_CALL(*mock_controller_ptr_, StartFileUploadFlow(_, _, _)).Times(0);

  ContextualSearchSessionHandle::DriveUploadParams params;
  params.drive_id = "id";
  params.resource_key = "key";
  params.mime_type = "type";
  params.file_name = "name";
  handle_->StartDriveContextUploadFlow(token, params);
}

TEST_F(ContextualSearchSessionHandleTest,
       StartUrlContextUploadFlow_DoesNotSetUploadType) {
  base::UnguessableToken token = handle_->CreateContextToken();
  std::string test_url = "https://www.google.com";

  EXPECT_CALL(*mock_controller_ptr_, StartFileUploadFlow(token, _, _))
      .WillOnce([&](const base::UnguessableToken& file_token,
                    std::unique_ptr<lens::ContextualInputData> input_data,
                    std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(input_data->primary_content_type, lens::MimeType::kUnknown);
        EXPECT_EQ(input_data->parsed_url, test_url);
        EXPECT_FALSE(input_data->upload_type.has_value());
      });

  handle_->StartUrlContextUploadFlow(token, test_url);
}

TEST_F(ContextualSearchSessionHandleTest,
       StartModalityChipUploadFlow_DoesNotSetUploadType) {
  base::UnguessableToken token = handle_->CreateContextToken();
  auto modality_chip_props = std::make_unique<lens::ModalityChipProps>();

  EXPECT_CALL(*mock_controller_ptr_, StartFileUploadFlow(token, _, _))
      .WillOnce([&](const base::UnguessableToken& file_token,
                    std::unique_ptr<lens::ContextualInputData> input_data,
                    std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_FALSE(input_data->upload_type.has_value());
      });

  handle_->StartModalityChipUploadFlow(token, std::move(modality_chip_props));
}

TEST_F(ContextualSearchSessionHandleTest, PreviousTurnsAppended) {
  EXPECT_TRUE(handle_->previous_turns().empty());

  contextual_tasks::ThreadTurn turn1;
  turn1.query = "first query";
  handle_->AddThreadTurn(turn1);
  ASSERT_EQ(handle_->previous_turns().size(), 1u);
  EXPECT_EQ(handle_->previous_turns()[0].query, "first query");

  contextual_tasks::ThreadTurn turn2;
  turn2.query = "second query";
  handle_->AddThreadTurn(turn2);
  ASSERT_EQ(handle_->previous_turns().size(), 2u);
  EXPECT_EQ(handle_->previous_turns()[0].query, "first query");
  EXPECT_EQ(handle_->previous_turns()[1].query, "second query");
}

TEST_F(ContextualSearchSessionHandleTest, GetSubmittedContextTabTitles) {
  base::UnguessableToken token1 = handle_->CreateContextToken();
  base::UnguessableToken token2 = handle_->CreateContextToken();

  FileInfo file_info1;
  file_info1.file_token = token1;
  file_info1.tab_title = "title 1";

  FileInfo file_info2;
  file_info2.file_token = token2;

  EXPECT_CALL(*mock_controller_ptr_, GetFileInfo(token1))
      .WillRepeatedly(testing::Return(&file_info1));
  EXPECT_CALL(*mock_controller_ptr_, GetFileInfo(token2))
      .WillRepeatedly(testing::Return(&file_info2));

  handle_->set_submitted_context_tokens({token1, token2});

  std::vector<std::string> tab_titles = handle_->GetSubmittedContextTabTitles();
  ASSERT_EQ(tab_titles.size(), 1u);
  EXPECT_EQ(tab_titles[0], "title 1");
}

TEST_F(ContextualSearchSessionHandleTest,
       NotifyQuerySubmittedSessionState_TabAttachmentCount) {
  base::HistogramTester histogram_tester;

  FileInfo tab_info;
  tab_info.file_token = base::UnguessableToken::Create();
  tab_info.mime_type = lens::MimeType::kAnnotatedPageContent;
  tab_info.tab_url = GURL("https://www.google.com");

  std::vector<FileInfo> file_infos = {tab_info};

  handle_->NotifyQuerySubmittedSessionState(file_infos, /*query_text_length=*/5);

  histogram_tester.ExpectUniqueSample(
      "ContextualSearch.Query.AttachmentCount.Tab.Unknown", 1, 1);
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_PopulatesRemovedContexts_InvalidInBrowser) {
  // Enable the feature to keep tabs in `uploaded_context_tokens_` across turns.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {omnibox::kContextManagementInComposebox,
       lens::features::kLensDeleteContextOnPageNavigation},
      {});

  auto mock_validator = std::make_unique<MockTabValidator>();
  MockTabValidator* mock_validator_ptr = mock_validator.get();

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Create a tab context token.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();

  FileInfo tab_file_info;
  tab_file_info.file_token = tab_token;
  tab_file_info.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(12345);
  tab_file_info.request_id = req_id;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_file_info));

  // --- First submission (Query 1) ---
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // --- Second submission (Query 2) ---
  // Tab A is still in uploaded_context_tokens_, but now invalid in browser.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info)))
      .WillRepeatedly(testing::Return(false));

  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Verify removed_contexts includes the request ID for the deleted tab.
  // Note: `persisted_tabs_` must NOT be erased during `DeleteFile` so its
  // `request_id` remains available here for `removed_contexts`.
  ASSERT_TRUE(captured_info);
  ASSERT_EQ(captured_info->removed_contexts.size(), 1u);
  EXPECT_EQ(captured_info->removed_contexts[0].uuid(), 12345u);
}

TEST_F(
    ContextualSearchSessionHandleTest,
    CreateClientToAimRequest_SmartTabSharingToggledOff_AddsUploadedContextIdsToRemovedContexts) {
  auto mock_validator = std::make_unique<MockTabValidator>();
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  local_handle->set_smart_tab_sharing_active(true);
  local_handle->set_smart_tab_sharing_toggled_since_last_turn(false);

  // Create a tab context token.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();

  FileInfo tab_file_info;
  tab_file_info.file_token = tab_token;
  tab_file_info.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(99999);
  tab_file_info.request_id = req_id;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_file_info));

  // Submit Query 1.
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // SmartTabSharing explicitly toggled off prior to Query 2.
  local_handle->set_smart_tab_sharing_active(false);

  // Verify that submitted context tokens and persisted tabs are cleared immediately on toggle.
  EXPECT_TRUE(local_handle->GetSubmittedContextTokens().empty());
  EXPECT_TRUE(local_handle->GetSubmittedContextFileInfos().empty());
  EXPECT_TRUE(local_handle->persisted_tabs().empty());
  EXPECT_EQ(local_handle->sts_toggled_removed_contexts().size(), 1u);

  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Assert: removed_contexts contains the uploaded context ID.
  ASSERT_TRUE(captured_info);
  ASSERT_EQ(captured_info->removed_contexts.size(), 1u);
  EXPECT_EQ(captured_info->removed_contexts[0].uuid(), 99999u);

  // Subsequent turn (Query 3): verify flag is reset and removed_contexts is not re-populated.
  auto request_info3 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info3;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info3 = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info3));

  ASSERT_TRUE(captured_info3);
  EXPECT_TRUE(captured_info3->removed_contexts.empty());
}

TEST_F(
    ContextualSearchSessionHandleTest,
    CreateClientToAimRequest_SmartTabSharingToggledOn_AddsUploadedContextIdsToRemovedContexts) {
  auto mock_validator = std::make_unique<MockTabValidator>();
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);
  local_handle->set_smart_tab_sharing_active(false);
  local_handle->set_smart_tab_sharing_toggled_since_last_turn(false);

  // Create a tab context token.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();

  FileInfo tab_file_info;
  tab_file_info.file_token = tab_token;
  tab_file_info.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(99999);
  tab_file_info.request_id = req_id;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_file_info));

  // Submit Query 1 with SmartTabSharing inactive by default.
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // SmartTabSharing explicitly toggled on prior to Query 2.
  local_handle->set_smart_tab_sharing_active(true);

  // Verify that submitted context tokens and persisted tabs are cleared immediately on toggle.
  EXPECT_TRUE(local_handle->GetSubmittedContextTokens().empty());
  EXPECT_TRUE(local_handle->GetSubmittedContextFileInfos().empty());
  EXPECT_TRUE(local_handle->persisted_tabs().empty());
  EXPECT_EQ(local_handle->sts_toggled_removed_contexts().size(), 1u);

  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Assert: removed_contexts contains the uploaded context ID.
  ASSERT_TRUE(captured_info);
  ASSERT_EQ(captured_info->removed_contexts.size(), 1u);
  EXPECT_EQ(captured_info->removed_contexts[0].uuid(), 99999u);

  // Subsequent turn (Query 3): verify flag is reset and removed_contexts is not
  // re-populated.
  auto request_info3 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info3;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info3 = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info3));

  ASSERT_TRUE(captured_info3);
  EXPECT_TRUE(captured_info3->removed_contexts.empty());
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_FlagDisabled_TabOpen_DoesNotSignalDeletion) {
  // Disable the feature so tabs are cleared from uploaded_context_tokens_
  // (simulating flag disabled).
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {lens::features::kLensDeleteContextOnPageNavigation},
      {omnibox::kContextManagementInComposebox});

  auto mock_validator = std::make_unique<MockTabValidator>();
  MockTabValidator* mock_validator_ptr = mock_validator.get();

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Create a tab context token.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();

  FileInfo tab_file_info;
  tab_file_info.file_token = tab_token;
  tab_file_info.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(12345);
  tab_file_info.request_id = req_id;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_file_info));

  // --- First submission (Query 1) ---
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // --- Second submission (Query 2) ---
  // Tab A is still open in browser.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info)))
      .WillOnce(testing::Return(true));

  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  ASSERT_TRUE(captured_info);
  EXPECT_TRUE(captured_info->removed_contexts.empty());
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_FlagDisabled_TabClosed_SignalsDeletion) {
  // Disable the feature so tabs are cleared from uploaded_context_tokens_
  // (simulating flag disabled).
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {lens::features::kLensDeleteContextOnPageNavigation},
      {omnibox::kContextManagementInComposebox});

  auto mock_validator = std::make_unique<MockTabValidator>();
  MockTabValidator* mock_validator_ptr = mock_validator.get();

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Create a tab context token.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();

  FileInfo tab_file_info;
  tab_file_info.file_token = tab_token;
  tab_file_info.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(12345);
  tab_file_info.request_id = req_id;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_file_info));

  // --- First submission (Query 1) ---
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // --- Second submission (Query 2) ---
  // Tab A is closed in browser.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info)))
      .WillOnce(testing::Return(false));

  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  ASSERT_TRUE(captured_info);
  ASSERT_EQ(captured_info->removed_contexts.size(), 1u);
  EXPECT_EQ(captured_info->removed_contexts[0].uuid(), 12345u);
}

TEST_F(
    ContextualSearchSessionHandleTest,
    CreateClientToAimRequest_Recontextualization_DeletesOldTokensButDoesNotDeleteTabInServer) {
  // Enable the feature to keep tabs in uploaded_context_tokens_ across turns.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {omnibox::kContextManagementInComposebox,
       lens::features::kLensDeleteContextOnPageNavigation},
      {});

  auto mock_validator = std::make_unique<MockTabValidator>();
  MockTabValidator* mock_validator_ptr = mock_validator.get();

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // 1. Upload Tab A (version 1).
  base::UnguessableToken tab_token1 = local_handle->CreateContextToken();
  FileInfo tab_file_info1;
  tab_file_info1.file_token = tab_token1;
  tab_file_info1.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id1;
  req_id1.set_uuid(12345);
  req_id1.set_sequence_id(1);
  tab_file_info1.request_id = req_id1;
  tab_file_info1.is_superceded = false;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token1))
      .WillRepeatedly(testing::Return(&tab_file_info1));

  // Submit Query 1.
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // 2. Recontextualize: Upload Tab A (version 2).
  // This marks version 1 as superceded.
  tab_file_info1.is_superceded = true;

  base::UnguessableToken tab_token2 = local_handle->CreateContextToken();
  FileInfo tab_file_info2;
  tab_file_info2.file_token = tab_token2;
  tab_file_info2.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id2;
  req_id2.set_uuid(12345);
  req_id2.set_sequence_id(2);
  tab_file_info2.request_id = req_id2;
  tab_file_info2.is_superceded = false;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token2))
      .WillRepeatedly(testing::Return(&tab_file_info2));

  // Validator should be called for the ACTIVE token (`tab_token2`).
  // Tab A is still valid in browser.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info2)))
      .WillOnce(testing::Return(true));
  // Validator should NOT be called for `tab_token1` because it is superceded.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info1)))
      .Times(0);

  // Submit Query 2.
  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Verify that NO deleted contexts are reported.
  ASSERT_TRUE(captured_info);
  EXPECT_TRUE(captured_info->removed_contexts.empty());

  // Verify that the navigated `tab_token1` is removed, and only `tab_token2`
  // remains in `persisted_tabs_` and `uploaded_context_tokens_` and
  // `submitted_context_tokens`.
  const auto& persisted_tabs = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs.size(), 1u);
  auto it = persisted_tabs.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it, persisted_tabs.end());
  EXPECT_EQ(it->second.first, tab_token2);
  EXPECT_THAT(local_handle->GetSubmittedContextTokens(),
              testing::UnorderedElementsAre(tab_token2));
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_Recontextualization_Invalid_DeletesOld) {
  // Enable the feature to keep tabs in `uploaded_context_tokens_` across turns.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {omnibox::kContextManagementInComposebox,
       lens::features::kLensDeleteContextOnPageNavigation},
      {});

  auto mock_validator = std::make_unique<MockTabValidator>();
  MockTabValidator* mock_validator_ptr = mock_validator.get();

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // 1. Upload Tab A (version 1).
  base::UnguessableToken tab_token1 = local_handle->CreateContextToken();
  FileInfo tab_file_info1;
  tab_file_info1.file_token = tab_token1;
  tab_file_info1.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id1;
  req_id1.set_uuid(12345);
  req_id1.set_sequence_id(1);
  tab_file_info1.request_id = req_id1;
  tab_file_info1.is_superceded = false;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token1))
      .WillRepeatedly(testing::Return(&tab_file_info1));

  // Submit Query 1.
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // 2. Recontextualize: Upload Tab A (version 2).
  tab_file_info1.is_superceded = true;

  base::UnguessableToken tab_token2 = local_handle->CreateContextToken();
  FileInfo tab_file_info2;
  tab_file_info2.file_token = tab_token2;
  tab_file_info2.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id2;
  req_id2.set_uuid(12345);
  req_id2.set_sequence_id(2);
  tab_file_info2.request_id = req_id2;
  tab_file_info2.is_superceded = false;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token2))
      .WillRepeatedly(testing::Return(&tab_file_info2));

  // Validator should be called for active token (`tab_token2`), and returns
  // false.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info2)))
      .WillRepeatedly(testing::Return(false));

  // Submit Query 2.
  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();

  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Verify that version 1 request ID is reported as deleted.
  ASSERT_TRUE(captured_info);
  ASSERT_EQ(captured_info->removed_contexts.size(), 1u);
  EXPECT_EQ(captured_info->removed_contexts[0].sequence_id(), 1);
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_TabClosed_RemovesFromSubmittedAndUploaded) {
  // Enable the feature to keep tabs in `uploaded_context_tokens_` across turns.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {omnibox::kContextManagementInComposebox,
       lens::features::kLensDeleteContextOnPageNavigation},
      {});

  auto mock_validator = std::make_unique<MockTabValidator>();
  MockTabValidator* mock_validator_ptr = mock_validator.get();

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // 1. Upload Tab A.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();
  FileInfo tab_file_info;
  tab_file_info.file_token = tab_token;
  tab_file_info.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(12345);
  req_id.set_sequence_id(1);
  tab_file_info.request_id = req_id;
  tab_file_info.is_superceded = false;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_file_info));

  // Submit Query 1.
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // Verify it is in `persisted_tabs` and submitted context tokens list, but
  // cleared from uploaded list.
  const auto& persisted_tabs = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs.size(), 1u);
  auto it = persisted_tabs.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it, persisted_tabs.end());
  EXPECT_EQ(it->second.first, tab_token);
  EXPECT_THAT(local_handle->GetUploadedContextTokens(),
              testing::Not(testing::Contains(tab_token)));
  EXPECT_THAT(local_handle->GetSubmittedContextTokens(),
              testing::Contains(tab_token));

  // 2. Tab is closed in browser.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info)))
      .WillRepeatedly(testing::Return(false));

  // Submit Query 2.
  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Verify it is removed from all lists.
  EXPECT_TRUE(local_handle->persisted_tabs().empty());
  EXPECT_THAT(local_handle->GetUploadedContextTokens(),
              testing::Not(testing::Contains(tab_token)));
  EXPECT_THAT(local_handle->GetSubmittedContextTokens(),
              testing::Not(testing::Contains(tab_token)));
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateSearchUrl_PopulatesPersistedTabs) {
  auto local_mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto* local_mock_controller_ptr = local_mock_controller.get();

  auto local_handle = service_->CreateSessionForTesting(
      std::move(local_mock_controller),
      std::make_unique<ContextualSearchMetricsRecorder>(
          ContextualSearchSource::kUnknown));
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  base::UnguessableToken token1 = local_handle->CreateContextToken();
  FileInfo file_info1;
  SessionID session_id1 = SessionID::NewUnique();
  file_info1.tab_session_id = session_id1;
  file_info1.tab_url = GURL("https://example.com");
  lens::LensOverlayRequestId req_id1;
  req_id1.set_sequence_id(1);
  file_info1.request_id = req_id1;
  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(token1))
      .WillRepeatedly(testing::Return(&file_info1));

  auto request_info = std::make_unique<
      ContextualSearchContextController::CreateSearchUrlRequestInfo>();
  request_info->file_tokens = {token1};

  EXPECT_CALL(*local_mock_controller_ptr, CreateSearchUrl(_, _));

  local_handle->CreateSearchUrl(std::move(request_info), base::DoNothing());

  // Verify `persisted_tabs_` has `token1`.
  const auto& persisted_tabs = local_handle->persisted_tabs();
  ASSERT_EQ(persisted_tabs.size(), 1u);
  auto it = persisted_tabs.find(session_id1);
  ASSERT_NE(it, persisted_tabs.end());
  EXPECT_EQ(it->second.first, token1);
  EXPECT_EQ(it->second.second.sequence_id(), 1);
}

TEST_F(ContextualSearchSessionHandleTest, IsTabToken_ValidatesTabSessionId) {
  auto local_mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto* local_mock_controller_ptr = local_mock_controller.get();

  auto local_handle = service_->CreateSessionForTesting(
      std::move(local_mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Token for a real browser tab (has valid `tab_session_id`).
  base::UnguessableToken tab_token = local_handle->CreateContextToken();
  FileInfo tab_file_info;
  tab_file_info.file_token = tab_token;
  tab_file_info.tab_session_id = SessionID::FromSerializedValue(1);

  // Token for a manual file (`tab_title` = "document.pdf", but no
  // `tab_session_id`).
  base::UnguessableToken file_token = local_handle->CreateContextToken();
  FileInfo file_info;
  file_info.file_token = file_token;
  file_info.tab_title = "document.pdf";

  // Token with invalid `tab_session_id`.
  base::UnguessableToken invalid_tab_token = local_handle->CreateContextToken();
  FileInfo invalid_tab_info;
  invalid_tab_info.file_token = invalid_tab_token;
  invalid_tab_info.tab_session_id = SessionID::InvalidValue();

  // Unknown token (not in controller).
  base::UnguessableToken unknown_token = base::UnguessableToken::Create();

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_file_info));
  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(file_token))
      .WillRepeatedly(testing::Return(&file_info));
  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(invalid_tab_token))
      .WillRepeatedly(testing::Return(&invalid_tab_info));
  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(unknown_token))
      .WillRepeatedly(testing::Return(nullptr));

  // Verify `IsTabToken` results.
  EXPECT_TRUE(local_handle->IsTabTokenForTesting(tab_token));
  EXPECT_FALSE(local_handle->IsTabTokenForTesting(file_token));
  EXPECT_FALSE(local_handle->IsTabTokenForTesting(invalid_tab_token));
  EXPECT_FALSE(local_handle->IsTabTokenForTesting(unknown_token));
}

TEST_F(ContextualSearchSessionHandleTest,
       DeleteTabContext_RestoredTab_Succeeds) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kContextManagementInComposebox,
      {{"enable_tab_deselection", "true"}});

  auto local_mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto* local_mock_controller_ptr = local_mock_controller.get();

  auto local_handle = service_->CreateSessionForTesting(
      std::move(local_mock_controller),
      std::make_unique<ContextualSearchMetricsRecorder>(
          ContextualSearchSource::kUnknown));
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Populate `persisted_tabs_` to mock a restored tab.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  SessionID session_id1 = SessionID::NewUnique();
  lens::LensOverlayRequestId req_id1;
  local_handle->set_persisted_tabs({{session_id1, {token1, req_id1}}});
  local_handle->set_submitted_context_tokens({token1});

  // Verify GetTokenForTab can find it.
  EXPECT_EQ(local_handle->GetTokenForTab(session_id1), token1);

  FileInfo file_info1;
  file_info1.file_token = token1;
  file_info1.tab_session_id = session_id1;
  file_info1.tab_url = GURL("https://example.com");
  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(token1))
      .WillRepeatedly(testing::Return(&file_info1));

  // Perform deletion of the restored tab.
  EXPECT_TRUE(local_handle->DeleteFile(token1));

  // Verify it is deselected.
  EXPECT_TRUE(local_handle->IsTabDeselected(session_id1,
                                            GURL("https://example.com"), ""));
}

TEST_F(ContextualSearchSessionHandleTest,
       DeleteFile_UploadedTabBeforeSubmissionPreventsPersistence) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      omnibox::kContextManagementInComposebox);

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  base::UnguessableToken tab_token = local_handle->CreateContextToken();
  FileInfo tab_info;
  tab_info.file_token = tab_token;
  tab_info.tab_session_id = SessionID::FromSerializedValue(1);

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_info));
  EXPECT_CALL(*mock_controller_ptr, DeleteFile(tab_token))
      .WillOnce(testing::Return(true));

  // Deleting uploaded tab BEFORE submission REMOVES it from
  // uploaded tokens and controller.
  EXPECT_TRUE(local_handle->DeleteFile(tab_token));
  EXPECT_TRUE(local_handle->GetUploadedContextTokens().empty());

  // Subsequent CreateClientToAimRequest should not persist this tab.
  auto request_info = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  local_handle->CreateClientToAimRequest(std::move(request_info));

  EXPECT_TRUE(local_handle->persisted_tabs().empty());
}

TEST_F(ContextualSearchSessionHandleTest,
       DeleteFile_UploadedFileDeletionDoesNotAffectExistingPersistentTabs) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      omnibox::kContextManagementInComposebox);

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Turn 1: Submit Tab A.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();
  FileInfo tab_info;
  tab_info.file_token = tab_token;
  tab_info.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id;
  req_id.set_sequence_id(1);
  tab_info.request_id = req_id;

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_info));

  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // Tab A should persist.
  const auto& persisted_tabs = local_handle->persisted_tabs();
  ASSERT_EQ(persisted_tabs.size(), 1u);
  auto it = persisted_tabs.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it, persisted_tabs.end());
  EXPECT_EQ(it->second.first, tab_token);

  // Turn 2: Upload image file.
  base::UnguessableToken image_token = local_handle->CreateContextToken();
  FileInfo image_info;
  image_info.file_token = image_token;

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(image_token))
      .WillRepeatedly(testing::Return(&image_info));
  EXPECT_CALL(*mock_controller_ptr, DeleteFile(image_token))
      .WillOnce(testing::Return(true));

  // Delete image file.
  EXPECT_TRUE(local_handle->DeleteFile(image_token));
  EXPECT_TRUE(local_handle->GetUploadedContextTokens().empty());

  // Persistent tab A should remain.
  const auto& persisted_tabs2 = local_handle->persisted_tabs();
  ASSERT_EQ(persisted_tabs2.size(), 1u);
  auto it2 = persisted_tabs2.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it2, persisted_tabs2.end());
  EXPECT_EQ(it2->second.first, tab_token);

  // Clear files with query_submitted=false. Persistent tab should remain.
  local_handle->ClearFiles(/*query_submitted=*/false);
  const auto& persisted_tabs3 = local_handle->persisted_tabs();
  ASSERT_EQ(persisted_tabs3.size(), 1u);
}

TEST_F(ContextualSearchSessionHandleTest,
       DeleteFile_UnuploadedTokenNotInControllerReturnsFalse) {
  base::UnguessableToken random_token = base::UnguessableToken::Create();

  EXPECT_CALL(*mock_controller_ptr_, GetFileInfo(random_token))
      .WillRepeatedly(testing::Return(nullptr));

  EXPECT_CALL(*mock_controller_ptr_, DeleteFile(_)).Times(0);

  EXPECT_FALSE(handle_->DeleteFile(random_token));
}

TEST_F(ContextualSearchSessionHandleTest,
       GetSubmittedContextFileInfos_FiltersDeselectedTabs) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kContextManagementInComposebox,
      {{"enable_tab_deselection", "true"}});

  auto local_mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto* local_mock_controller_ptr = local_mock_controller.get();

  auto local_handle = service_->CreateSessionForTesting(
      std::move(local_mock_controller),
      std::make_unique<ContextualSearchMetricsRecorder>(
          ContextualSearchSource::kUnknown));
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  SessionID session_id1 = SessionID::NewUnique();
  lens::LensOverlayRequestId req_id1;
  local_handle->set_persisted_tabs({{session_id1, {token1, req_id1}}});
  local_handle->set_submitted_context_tokens({token1});

  FileInfo file_info1;
  file_info1.file_token = token1;
  file_info1.tab_session_id = session_id1;
  file_info1.tab_url = GURL("https://example.com");
  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(token1))
      .WillRepeatedly(testing::Return(&file_info1));

  // Initially it should be returned.
  EXPECT_EQ(local_handle->GetSubmittedContextFileInfos().size(), 1u);

  // Deselect it.
  EXPECT_TRUE(local_handle->DeleteFile(token1));

  // Now it should be filtered out.
  EXPECT_EQ(local_handle->GetSubmittedContextFileInfos().size(), 0u);
}

TEST_F(ContextualSearchSessionHandleTest, IsTabDeselected_ClearsOnNavigation) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kContextManagementInComposebox,
      {{"enable_tab_deselection", "true"}});

  auto local_mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto* local_mock_controller_ptr = local_mock_controller.get();

  auto local_handle = service_->CreateSessionForTesting(
      std::move(local_mock_controller),
      std::make_unique<ContextualSearchMetricsRecorder>(
          ContextualSearchSource::kUnknown));
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  base::UnguessableToken token1 = local_handle->CreateContextToken();
  SessionID session_id1 = SessionID::NewUnique();
  FileInfo file_info1;
  file_info1.file_token = token1;
  file_info1.tab_session_id = session_id1;
  file_info1.tab_url = GURL("https://google.com");
  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(token1))
      .WillRepeatedly(testing::Return(&file_info1));
  EXPECT_CALL(*local_mock_controller_ptr, DeleteFile(token1))
      .WillOnce(testing::Return(true));

  // Deselect the tab context.
  EXPECT_TRUE(local_handle->DeleteFile(token1));

  // Tab is deselected for the old URL.
  EXPECT_TRUE(local_handle->IsTabDeselected(session_id1,
                                            GURL("https://google.com"), ""));

  // Tab is NOT deselected if it navigated to a new URL (should lazily clear
  // deselection).
  EXPECT_FALSE(local_handle->IsTabDeselected(
      session_id1, GURL("https://wikipedia.org"), ""));

  // Verify that querying with the old URL again also returns false (since it
  // was cleared).
  EXPECT_FALSE(local_handle->IsTabDeselected(session_id1,
                                             GURL("https://google.com"), ""));
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_RemovesPersistentTabOnNavigationOrClose) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {omnibox::kContextManagementInComposebox,
       lens::features::kLensDeleteContextOnPageNavigation},
      {});

  auto mock_validator = std::make_unique<MockTabValidator>();
  MockTabValidator* mock_validator_ptr = mock_validator.get();

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Upload Tab A and Tab B.
  base::UnguessableToken tab_token_a = local_handle->CreateContextToken();
  FileInfo tab_file_info_a;
  tab_file_info_a.file_token = tab_token_a;
  tab_file_info_a.tab_session_id = SessionID::FromSerializedValue(1);
  lens::LensOverlayRequestId req_id_a;
  req_id_a.set_sequence_id(1);
  tab_file_info_a.request_id = req_id_a;

  base::UnguessableToken tab_token_b = local_handle->CreateContextToken();
  FileInfo tab_file_info_b;
  tab_file_info_b.file_token = tab_token_b;
  tab_file_info_b.tab_session_id = SessionID::FromSerializedValue(2);
  lens::LensOverlayRequestId req_id_b;
  req_id_b.set_sequence_id(2);
  tab_file_info_b.request_id = req_id_b;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token_a))
      .WillRepeatedly(testing::Return(&tab_file_info_a));
  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token_b))
      .WillRepeatedly(testing::Return(&tab_file_info_b));

  // Submit Query 1.
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  const auto& persisted_tabs = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs.size(), 2u);

  // Tab A navigated away or closed, Tab B remains open.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info_a)))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info_b)))
      .WillRepeatedly(testing::Return(true));

  // Submit Query 2.
  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Tab A should be removed from `persisted_tabs_`, Tab B should remain.
  const auto& persisted_tabs2 = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs2.size(), 1u);
  auto it = persisted_tabs2.find(SessionID::FromSerializedValue(2));
  ASSERT_NE(it, persisted_tabs2.end());
  EXPECT_EQ(it->second.first, tab_token_b);
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_UserRemovedTab_SignalsDeletion) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kContextManagementInComposebox,
      {{"enable_tab_deselection", "true"}});

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* local_mock_controller_ptr =
      mock_controller.get();

  auto mock_validator = std::make_unique<MockTabValidator>();
  MockTabValidator* mock_validator_ptr = mock_validator.get();
  auto local_service = std::make_unique<ContextualSearchService>(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      std::move(mock_validator));

  auto local_handle = local_service->CreateSessionForTesting(
      std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);
  EXPECT_CALL(*mock_validator_ptr, AreUrlsEquivalent(_, _, _, _))
      .WillRepeatedly(testing::Return(true));

  // 1. Create a tab context token.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();

  FileInfo tab_file_info;
  tab_file_info.file_token = tab_token;
  tab_file_info.tab_session_id = SessionID::FromSerializedValue(1);
  tab_file_info.tab_url = GURL("https://google.com");
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(12345);
  tab_file_info.request_id = req_id;

  EXPECT_CALL(*local_mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_file_info));

  // 2. Submit Turn 1.
  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // 3. User explicitly deletes tab context.
  EXPECT_TRUE(local_handle->DeleteFile(tab_token));
  EXPECT_TRUE(local_handle->IsTabDeselected(SessionID::FromSerializedValue(1),
                                            GURL("https://google.com"), ""));

  // 4. Submit Turn 2.
  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*local_mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  ASSERT_TRUE(captured_info);
  ASSERT_EQ(captured_info->removed_contexts.size(), 1u);
  EXPECT_EQ(captured_info->removed_contexts[0].uuid(), 12345u);
}

TEST_F(ContextualSearchSessionHandleTest,
       DeleteFile_RecontextualizedTab_RemovesAllTokens) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kContextManagementInComposebox,
      {{"enable_tab_deselection", "true"}});

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Submit Turn 1: Tab A (creating token 1).
  base::UnguessableToken tab_token1 = local_handle->CreateContextToken();
  FileInfo tab_info1;
  tab_info1.file_token = tab_token1;
  tab_info1.tab_session_id = SessionID::FromSerializedValue(1);
  tab_info1.request_id = lens::LensOverlayRequestId();

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token1))
      .WillRepeatedly(testing::Return(&tab_info1));

  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  // Turn 2: Re-contextualize Tab A (thus adding token 2 (new navigated page)
  // for tab A).
  base::UnguessableToken tab_token2 = local_handle->CreateContextToken();
  FileInfo tab_info2;
  tab_info2.file_token = tab_token2;
  tab_info2.tab_session_id = SessionID::FromSerializedValue(1);
  tab_info2.request_id = lens::LensOverlayRequestId();

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token2))
      .WillRepeatedly(testing::Return(&tab_info2));

  // User explicitly deletes Tab A (deleting active token 2).
  // When deleting tab, Token 2 (unsubmitted) is deleted from
  // controller since that token was never submitted. Token 1
  // (which was submitted) is removed from `submitted_context_tokens_`
  // but retained in controller for metadata. `persisted_tabs_` is retained
  // until CreateClientToAimRequest extracts request_id for removed_contexts.
  EXPECT_CALL(*mock_controller_ptr, DeleteFile(tab_token2))
      .WillOnce(testing::Return(true));

  EXPECT_TRUE(local_handle->DeleteFile(tab_token2));

  // Verify associated tokens are cleared due to deleted tab.
  EXPECT_FALSE(local_handle->persisted_tabs().empty());
  EXPECT_TRUE(local_handle->GetUploadedContextTokens().empty());
  EXPECT_TRUE(local_handle->GetSubmittedContextTokens().empty());
  EXPECT_FALSE(
      local_handle->IsTabInContext(SessionID::FromSerializedValue(1)));

  // Turn 2: Create request to send deletion signal to server.
  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_info;
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [&](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                  info) {
            captured_info = std::move(info);
            return lens::ClientToAimMessage();
          });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Verify persisted_tabs is now cleared and request_id was sent in
  // removed_contexts.
  ASSERT_TRUE(captured_info);
  ASSERT_EQ(captured_info->removed_contexts.size(), 1u);
  EXPECT_TRUE(local_handle->persisted_tabs().empty());
}

TEST_F(ContextualSearchSessionHandleTest,
       ClearFiles_PersistsTabsOnlyOnSubmission) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      omnibox::kContextManagementInComposebox);

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Create tab token and file token.
  base::UnguessableToken tab_token = local_handle->CreateContextToken();
  base::UnguessableToken file_token = local_handle->CreateContextToken();

  FileInfo tab_info;
  tab_info.file_token = tab_token;
  tab_info.tab_session_id = SessionID::FromSerializedValue(1);
  tab_info.request_id = lens::LensOverlayRequestId();

  FileInfo file_info;
  file_info.file_token = file_token;

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_info));
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(file_token))
      .WillRepeatedly(testing::Return(&file_info));

  EXPECT_THAT(local_handle->GetUploadedContextTokens(),
              testing::UnorderedElementsAre(tab_token, file_token));
  EXPECT_TRUE(local_handle->persisted_tabs().empty());

  auto request_info = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  local_handle->CreateClientToAimRequest(std::move(request_info));

  // Uploaded tokens should be completely cleared.
  EXPECT_TRUE(local_handle->GetUploadedContextTokens().empty());
  // Tab token should be moved to `persisted_tabs_`, file token should NOT be.
  const auto& persisted_tabs = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs.size(), 1u);
  auto it = persisted_tabs.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it, persisted_tabs.end());
  EXPECT_EQ(it->second.first, tab_token);

  local_handle->ClearFiles(/*query_submitted=*/false);
  EXPECT_TRUE(local_handle->GetUploadedContextTokens().empty());
  // Verify that submitted tabs are NOT cleared when "query_submitted=false"
  // (intended behavior).
  const auto& persisted_tabs2 = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs2.size(), 1u);
  auto it2 = persisted_tabs2.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it2, persisted_tabs2.end());
  EXPECT_EQ(it2->second.first, tab_token);
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_DoesNotReattachPersistedTabsOnFollowUpTurn) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      omnibox::kContextManagementInComposebox);

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Turn 1: Submit Tab A (token 1). Token 1 is moved to `persisted_tabs_`.
  base::UnguessableToken tab_token1 = local_handle->CreateContextToken();
  FileInfo tab_info1;
  tab_info1.file_token = tab_token1;
  tab_info1.tab_session_id = SessionID::FromSerializedValue(1);
  tab_info1.request_id = lens::LensOverlayRequestId();

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token1))
      .WillRepeatedly(testing::Return(&tab_info1));

  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  std::vector<base::UnguessableToken> turn1_file_tokens;
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce([&turn1_file_tokens](
                    std::unique_ptr<ContextualSearchContextController::
                                        CreateClientToAimRequestInfo> info) {
        turn1_file_tokens = info->file_tokens;
        return lens::ClientToAimMessage();
      });

  local_handle->CreateClientToAimRequest(std::move(request_info1));
  EXPECT_THAT(turn1_file_tokens, testing::ElementsAre(tab_token1));
  const auto& persisted_tabs = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs.size(), 1u);
  auto it = persisted_tabs.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it, persisted_tabs.end());
  EXPECT_EQ(it->second.first, tab_token1);

  // Turn 2: Follow-up query without uploading any new tabs or files.
  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  std::vector<base::UnguessableToken> turn2_file_tokens;
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce([&turn2_file_tokens](
                    std::unique_ptr<ContextualSearchContextController::
                                        CreateClientToAimRequestInfo> info) {
        turn2_file_tokens = info->file_tokens;
        return lens::ClientToAimMessage();
      });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // Verify turn 2 `file_tokens` is empty (persisted tab is not re-attached).
  EXPECT_TRUE(turn2_file_tokens.empty());
  // Verify persisted tabs still retains Tab A for UI tracking.
  const auto& persisted_tabs2 = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs2.size(), 1u);
  auto it2 = persisted_tabs2.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it2, persisted_tabs2.end());
  EXPECT_EQ(it2->second.first, tab_token1);
}

TEST_F(
    ContextualSearchSessionHandleTest,
    CreateClientToAimRequest_AddsAndDeduplicatesPersistentTabsInSubmittedTokens) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      omnibox::kContextManagementInComposebox);

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Turn 1: Submit Tab A (token 1). Token 1 is moved to `persisted_tabs_`.
  base::UnguessableToken tab_token1 = local_handle->CreateContextToken();
  FileInfo tab_info1;
  tab_info1.file_token = tab_token1;
  tab_info1.tab_session_id = SessionID::FromSerializedValue(1);
  tab_info1.request_id = lens::LensOverlayRequestId();

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token1))
      .WillRepeatedly(testing::Return(&tab_info1));

  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(
          [](std::unique_ptr<
              ContextualSearchContextController::CreateClientToAimRequestInfo>
                 info) { return lens::ClientToAimMessage(); });
  local_handle->CreateClientToAimRequest(std::move(request_info1));

  const auto& persisted_tabs = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs.size(), 1u);
  auto it = persisted_tabs.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it, persisted_tabs.end());
  EXPECT_EQ(it->second.first, tab_token1);

  // Turn 2: Re-contextualize Tab A (token 2) + attach new image file.
  // Token 2 should replace token 1 in `persisted_tabs_`.
  base::UnguessableToken tab_token2 = local_handle->CreateContextToken();
  FileInfo tab_info2;
  tab_info2.file_token = tab_token2;
  tab_info2.tab_session_id = SessionID::FromSerializedValue(1);
  tab_info2.request_id = lens::LensOverlayRequestId();

  base::UnguessableToken image_token = local_handle->CreateContextToken();
  FileInfo image_info;
  image_info.file_token = image_token;

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token2))
      .WillRepeatedly(testing::Return(&tab_info2));
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(image_token))
      .WillRepeatedly(testing::Return(&image_info));

  auto request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  std::vector<base::UnguessableToken> submitted_file_tokens;
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce([&submitted_file_tokens](
                    std::unique_ptr<ContextualSearchContextController::
                                        CreateClientToAimRequestInfo> info) {
        submitted_file_tokens = info->file_tokens;
        return lens::ClientToAimMessage();
      });

  local_handle->CreateClientToAimRequest(std::move(request_info2));

  // File tokens set must contain `tab_token2` and `image_token`, and NOT
  // `tab_token1`, as that is superceded.
  EXPECT_THAT(submitted_file_tokens,
              testing::UnorderedElementsAre(tab_token2, image_token));

  // Persisted tab tokens should now contain `tab_token2` (no `image_token`, no
  // `tab_token1`).
  const auto& persisted_tabs2 = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs2.size(), 1u);
  auto it2 = persisted_tabs2.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it2, persisted_tabs2.end());
  EXPECT_EQ(it2->second.first, tab_token2);
}

TEST_F(ContextualSearchSessionHandleTest,
       GetSuggestInputs_DeduplicatesSubmittedTabTokensUponRecontextualization) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      omnibox::kContextManagementInComposebox);

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Turn 1: Submit Tab A (token 1). Token 1 is moved to `persisted_tabs_`.
  base::UnguessableToken tab_token1 = local_handle->CreateContextToken();
  FileInfo tab_info1;
  tab_info1.file_token = tab_token1;
  tab_info1.tab_session_id = SessionID::FromSerializedValue(1);
  tab_info1.request_id = lens::LensOverlayRequestId();

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token1))
      .WillRepeatedly(testing::Return(&tab_info1));

  auto request_info1 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  local_handle->CreateClientToAimRequest(std::move(request_info1));
  const auto& persisted_tabs = local_handle->persisted_tabs();
  EXPECT_EQ(persisted_tabs.size(), 1u);
  auto it = persisted_tabs.find(SessionID::FromSerializedValue(1));
  ASSERT_NE(it, persisted_tabs.end());
  EXPECT_EQ(it->second.first, tab_token1);

  // Turn 2: Re-contextualize Tab A (token 2) for same `tab_session_id`. Token
  // 1 is removed in favor of token 2 due to token 1 being superceded.
  base::UnguessableToken tab_token2 = local_handle->CreateContextToken();
  FileInfo tab_info2;
  tab_info2.file_token = tab_token2;
  tab_info2.tab_session_id = SessionID::FromSerializedValue(1);

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token2))
      .WillRepeatedly(testing::Return(&tab_info2));

  // `CreateSuggestInputs` should only receive `tab_token2`, not `tab_token1`.
  EXPECT_CALL(*mock_controller_ptr,
              CreateSuggestInputs(testing::ElementsAre(tab_token2)))
      .WillOnce(testing::Return(
          std::make_unique<lens::proto::LensOverlaySuggestInputs>()));

  local_handle->GetSuggestInputs();
}

TEST_F(ContextualSearchSessionHandleTest,
       GetActiveTokenForTab_IgnoresSupercededTokens) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      omnibox::kContextManagementInComposebox);

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  SessionID tab_session_id = SessionID::FromSerializedValue(1);

  // Scenario 1: A superceded token in `uploaded_context_tokens_`.
  base::UnguessableToken uploaded_token1 = local_handle->CreateContextToken();
  FileInfo uploaded_info1;
  uploaded_info1.file_token = uploaded_token1;
  uploaded_info1.tab_session_id = tab_session_id;
  uploaded_info1.is_superceded = true;

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(uploaded_token1))
      .WillRepeatedly(testing::Return(&uploaded_info1));

  // `GetActiveTokenForTab` should ignore the superceded token and return empty.
  EXPECT_TRUE(
      local_handle->GetActiveTokenForTabForTesting(tab_session_id).is_empty());

  // Scenario 2: An active (non-superceded) token in `uploaded_context_tokens_`.
  base::UnguessableToken uploaded_token2 = local_handle->CreateContextToken();
  FileInfo uploaded_info2;
  uploaded_info2.file_token = uploaded_token2;
  uploaded_info2.tab_session_id = tab_session_id;
  uploaded_info2.is_superceded = false;

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(uploaded_token2))
      .WillRepeatedly(testing::Return(&uploaded_info2));

  // Should return `uploaded_token2` since it is not superceded.
  EXPECT_EQ(local_handle->GetActiveTokenForTabForTesting(tab_session_id),
            uploaded_token2);
}

TEST_F(ContextualSearchSessionHandleTest,
       GetActiveTokenForTab_FlagDisabled_IgnoresPersistedTabs) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(
      omnibox::kContextManagementInComposebox);

  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  SessionID tab_session_id = SessionID::FromSerializedValue(1);

  // Manually add a token to `persisted_tabs_`.
  ContextualSearchSessionHandle::PersistedTabsMap persisted_map;
  base::UnguessableToken persisted_token = base::UnguessableToken::Create();
  persisted_map[tab_session_id] =
      std::make_pair(persisted_token, lens::LensOverlayRequestId());
  local_handle->set_persisted_tabs(persisted_map);

  // Because the flag is disabled, `GetActiveTokenForTab` should ignore
  // `persisted_tabs_` and return an empty token.
  EXPECT_TRUE(
      local_handle->GetActiveTokenForTabForTesting(tab_session_id).is_empty());
}

TEST_F(ContextualSearchSessionHandleTest,
       MaybeAddTabToPersistedTabs_IgnoresSupercededTokens) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  base::UnguessableToken tab_token = local_handle->CreateContextToken();
  FileInfo tab_info;
  tab_info.file_token = tab_token;
  tab_info.tab_session_id = SessionID::FromSerializedValue(1);
  tab_info.request_id = lens::LensOverlayRequestId();
  tab_info.is_superceded = true;

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token))
      .WillRepeatedly(testing::Return(&tab_info));

  // Submit Query 1. Because token is superceded, `MaybeAddTabToPersistedTabs`
  // should skip it.
  auto request_info = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  local_handle->CreateClientToAimRequest(std::move(request_info));

  // Verify persisted tabs is empty.
  EXPECT_TRUE(local_handle->persisted_tabs().empty());
}

TEST_F(ContextualSearchSessionHandleTest, HasSubmittedContext) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto local_handle =
      service_->CreateSessionForTesting(std::move(mock_controller), nullptr);
  local_handle->CheckSearchContentSharingSettings(&prefs_);

  // Initially false.
  EXPECT_FALSE(local_handle->has_submitted_context());

  // Uploading context without submitting query does not set
  // has_submitted_context.
  base::UnguessableToken file_token = local_handle->CreateContextToken();
  EXPECT_FALSE(local_handle->has_submitted_context());

  // Submitting a query with file tokens sets has_submitted_context to true.
  auto request_info = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  request_info->file_tokens.push_back(file_token);

  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  local_handle->CreateClientToAimRequest(std::move(request_info));

  EXPECT_TRUE(local_handle->has_submitted_context());

  // Clearing submitted context tokens does not reset has_submitted_context.
  local_handle->ClearSubmittedContextTokens();
  EXPECT_TRUE(local_handle->has_submitted_context());
}

}  // namespace contextual_search
