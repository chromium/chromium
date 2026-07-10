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

  // Expect StartFileUploadFlow to be called with kUnknown for SVG.
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

  ASSERT_TRUE(captured_info);
  ASSERT_EQ(captured_info->removed_contexts.size(), 1u);
  EXPECT_EQ(captured_info->removed_contexts[0].uuid(), 12345u);
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

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_Recontextualization_DoesNotDeleteOld) {
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

  // Validator should be called for the ACTIVE token (tab_token2).
  // Tab A is still valid in browser.
  EXPECT_CALL(*mock_validator_ptr,
              IsTabValidAndPointingToUrl(testing::Ref(tab_file_info2)))
      .WillOnce(testing::Return(true));
  // Validator should NOT be called for tab_token1 because it is superceded.
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
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateClientToAimRequest_Recontextualization_Invalid_DeletesOld) {
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

  // Validator should be called for active token (tab_token2), and returns
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

  // Verify it is in both lists.
  EXPECT_THAT(local_handle->GetUploadedContextTokens(),
              testing::Contains(tab_token));
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

  // Verify it is removed from both lists.
  EXPECT_THAT(local_handle->GetUploadedContextTokens(),
              testing::Not(testing::Contains(tab_token)));
  EXPECT_THAT(local_handle->GetSubmittedContextTokens(),
              testing::Not(testing::Contains(tab_token)));
}

TEST_F(ContextualSearchSessionHandleTest,
       CreateSearchUrl_PopulatesSubmittedTabs) {
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

  // Verify submitted_tabs_ has token1.
  const auto& submitted_tabs = local_handle->submitted_tabs();
  ASSERT_EQ(submitted_tabs.size(), 1u);
  auto it = submitted_tabs.find(session_id1);
  ASSERT_NE(it, submitted_tabs.end());
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

}  // namespace contextual_search
