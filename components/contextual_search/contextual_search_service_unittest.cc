// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_service.h"

#include "base/test/task_environment.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/fake_variations_client.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/variations_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/modality_chip_props.pb.h"

namespace contextual_search {

namespace {

using testing::_;
using testing::IsNull;
using testing::NotNull;

}  // namespace

class ContextualSearchServiceTest : public testing::Test {
 public:
  ContextualSearchServiceTest()
      : identity_test_env_(std::make_unique<signin::IdentityTestEnvironment>()),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    service_ = std::make_unique<ContextualSearchService>(
        identity_test_env_->identity_manager(), test_shared_loader_factory_,
        search_engines_test_environment_.template_url_service(),
        &fake_variations_client_, version_info::Channel::UNKNOWN, "en-US");
    ContextualSearchService::RegisterProfilePrefs(pref_service_.registry());
  }

  lens::ClientToAimMessage CaptureClientToAimRequest(
      std::unique_ptr<
          ContextualSearchContextController::CreateClientToAimRequestInfo>
          create_client_to_aim_request_info) {
    captured_client_to_aim_message_ =
        std::move(create_client_to_aim_request_info);
    return lens::ClientToAimMessage();
  }

  ContextualSearchContextController::CreateClientToAimRequestInfo*
  GetCapturedClientToAimRequest() {
    return captured_client_to_aim_message_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  FakeVariationsClient fake_variations_client_;
  std::unique_ptr<ContextualSearchService> service_;
  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_client_to_aim_message_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ContextualSearchServiceTest, Session) {
  // Try to get a session that does not exist.
  auto bad_handle = service_->GetSession(base::UnguessableToken::Create(),
                                         /*invocation_source=*/std::nullopt);
  ASSERT_THAT(bad_handle, IsNull());

  // Create a new session.
  auto config_params1 =
      std::make_unique<ContextualSearchContextController::ConfigParams>();
  config_params1->send_lns_surface = false;
  config_params1->suppress_lns_surface_param_if_no_image = true;
  config_params1->enable_viewport_images = false;
  auto session1_handle1 = service_->CreateSession(
      std::move(config_params1), ContextualSearchSource::kUnknown,
      /*invocation_source=*/std::nullopt);
  // Check the search content sharing settings to notify the session handle
  // that the client is properly checking the pref value.
  session1_handle1->CheckSearchContentSharingSettings(&pref_service_);
  ASSERT_THAT(session1_handle1, NotNull());
  ASSERT_THAT(session1_handle1->GetController(), NotNull());
  ASSERT_THAT(session1_handle1->GetMetricsRecorder(), NotNull());

  // Create another new session.
  auto config_params2 =
      std::make_unique<ContextualSearchContextController::ConfigParams>();
  config_params2->send_lns_surface = false;
  config_params2->suppress_lns_surface_param_if_no_image = true;
  config_params2->enable_viewport_images = false;
  auto session2_handle1 = service_->CreateSession(
      std::move(config_params2), ContextualSearchSource::kUnknown,
      /*invocation_source=*/std::nullopt);
  session2_handle1->CheckSearchContentSharingSettings(&pref_service_);
  ASSERT_THAT(session2_handle1, NotNull());
  ASSERT_THAT(session2_handle1->GetController(), NotNull());
  ASSERT_THAT(session2_handle1->GetMetricsRecorder(), NotNull());

  // Get a new handle to session two.
  auto session2_handle2 = service_->GetSession(
      session2_handle1->session_id(), /*invocation_source=*/std::nullopt);
  session2_handle2->CheckSearchContentSharingSettings(&pref_service_);
  ASSERT_THAT(session2_handle2, NotNull());
  EXPECT_EQ(session2_handle2->GetController(),
            session2_handle1->GetController());
  EXPECT_EQ(session2_handle2->GetMetricsRecorder(),
            session2_handle1->GetMetricsRecorder());
  EXPECT_NE(session1_handle1->GetMetricsRecorder(),
            session2_handle1->GetMetricsRecorder());
  EXPECT_NE(session1_handle1->GetController(),
            session2_handle1->GetController());

  // Release the first handle to session two. The session should still be alive.
  session2_handle1.reset();
  auto session2_handle3 = service_->GetSession(
      session2_handle2->session_id(), /*invocation_source=*/std::nullopt);
  session2_handle3->CheckSearchContentSharingSettings(&pref_service_);
  ASSERT_THAT(session2_handle3, NotNull());
  EXPECT_EQ(session2_handle3->GetController(),
            session2_handle2->GetController());
  EXPECT_EQ(session2_handle3->GetMetricsRecorder(),
            session2_handle2->GetMetricsRecorder());

  // Release the remaining handles to session two. The session should be
  // released.
  auto session_id = session2_handle2->session_id();
  session2_handle2.reset();
  session2_handle3.reset();
  auto session2_handle4 =
      service_->GetSession(session_id, /*invocation_source=*/std::nullopt);
  ASSERT_THAT(session2_handle4, IsNull());

  // Get a new handle to session one.
  auto session1_handle2 = service_->GetSession(
      session1_handle1->session_id(), /*invocation_source=*/std::nullopt);
  session1_handle2->CheckSearchContentSharingSettings(&pref_service_);
  ASSERT_THAT(session1_handle2, NotNull());
  EXPECT_EQ(session1_handle2->GetController(),
            session1_handle1->GetController());
  EXPECT_EQ(session1_handle2->GetMetricsRecorder(),
            session1_handle1->GetMetricsRecorder());
}

TEST_F(ContextualSearchServiceTest, PendingContextTokens) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
      ContextualSearchSource::kUnknown);

  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto session_handle = service_->CreateSessionForTesting(
      std::move(mock_controller), std::move(metrics_recorder));
  session_handle->CheckSearchContentSharingSettings(&pref_service_);
  session_handle->NotifySessionStarted();

  // Add some dummy tokens.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  session_handle->GetUploadedContextTokensForTesting().push_back(token1);
  session_handle->GetUploadedContextTokensForTesting().push_back(token2);

  // Capture the tokens before they are moved.
  std::vector<base::UnguessableToken> expected_request_tokens1;
  expected_request_tokens1.push_back(token1);
  expected_request_tokens1.push_back(token2);

  // Expect CreateClientToAimRequest to be called and capture the message.
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Invoke(
          this, &ContextualSearchServiceTest::CaptureClientToAimRequest));

  // Call CreateClientToAimRequest to move tokens to pending.
  auto create_client_to_aim_request_info = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  session_handle->CreateClientToAimRequest(
      std::move(create_client_to_aim_request_info));

  // Verify that the request contains the correct unguessable tokens.
  EXPECT_THAT(GetCapturedClientToAimRequest()->file_tokens,
              testing::UnorderedElementsAreArray(expected_request_tokens1));

  // Verify that submitted context tokens contains the moved tokens.
  std::vector<base::UnguessableToken> expected_submitted_tokens;
  expected_submitted_tokens.push_back(token1);
  expected_submitted_tokens.push_back(token2);
  EXPECT_THAT(session_handle->GetSubmittedContextTokens(),
              testing::UnorderedElementsAreArray(expected_submitted_tokens));
  EXPECT_TRUE(session_handle->GetUploadedContextTokens().empty());

  // Add more tokens and call CreateClientToAimRequest again.
  base::UnguessableToken token3 = base::UnguessableToken::Create();
  session_handle->GetUploadedContextTokensForTesting().push_back(token3);

  // Capture the tokens for the second request.
  std::vector<base::UnguessableToken> expected_request_tokens2;
  expected_request_tokens2.push_back(token3);

  // Expect CreateClientToAimRequest to be called again and capture the message.
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Invoke(
          this, &ContextualSearchServiceTest::CaptureClientToAimRequest));

  auto create_client_to_aim_request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  session_handle->CreateClientToAimRequest(
      std::move(create_client_to_aim_request_info2));

  // Verify that the second request contains only the newly added token.
  EXPECT_THAT(GetCapturedClientToAimRequest()->file_tokens,
              testing::UnorderedElementsAreArray(expected_request_tokens2));

  // Verify that the submitted context tokens has both the previous and the new
  // tokens.
  expected_submitted_tokens.push_back(token3);
  EXPECT_THAT(session_handle->GetSubmittedContextTokens(),
              testing::UnorderedElementsAreArray(expected_submitted_tokens));
  EXPECT_TRUE(session_handle->GetUploadedContextTokens().empty());

  // Clear the pending tokens.
  session_handle->ClearSubmittedContextTokens();

  // Verify that the submitted context tokens are empty.
  EXPECT_TRUE(session_handle->GetSubmittedContextTokens().empty());
}

TEST_F(ContextualSearchServiceTest, FileInfoTest) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
      ContextualSearchSource::kUnknown);

  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto session_handle = service_->CreateSessionForTesting(
      std::move(mock_controller), std::move(metrics_recorder));
  session_handle->CheckSearchContentSharingSettings(&pref_service_);
  session_handle->NotifySessionStarted();

  // Create tokens and FileInfo objects.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  FileInfo file_info1;
  file_info1.file_token = token1;
  file_info1.file_name = "file1.pdf";

  base::UnguessableToken token2 = base::UnguessableToken::Create();
  FileInfo file_info2;
  file_info2.file_token = token2;
  file_info2.file_name = "file2.jpg";

  base::UnguessableToken tab_token1 = base::UnguessableToken::Create();
  FileInfo tab_info1;
  tab_info1.file_token = tab_token1;
  tab_info1.tab_url = GURL("http://example.com/tab1");
  tab_info1.tab_title = "Tab 1 Title";
  tab_info1.tab_session_id = SessionID::FromSerializedValue(123);

  base::UnguessableToken tab_token2 = base::UnguessableToken::Create();
  FileInfo tab_info2;
  tab_info2.file_token = tab_token2;
  tab_info2.tab_url = GURL("http://example.com/tab2");
  tab_info2.tab_title = "Tab 2 Title";
  tab_info2.tab_session_id = SessionID::FromSerializedValue(456);

  // Mock GetFileInfo.
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token1))
      .WillRepeatedly(testing::Return(&file_info1));
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token2))
      .WillRepeatedly(testing::Return(&file_info2));
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token1))
      .WillRepeatedly(testing::Return(&tab_info1));
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(tab_token2))
      .WillRepeatedly(testing::Return(&tab_info2));

  // Test GetUploadedContextFileInfos.
  session_handle->GetUploadedContextTokensForTesting().push_back(token1);
  session_handle->GetUploadedContextTokensForTesting().push_back(tab_token1);
  std::vector<FileInfo> uploaded_infos =
      session_handle->GetUploadedContextFileInfos();
  ASSERT_EQ(uploaded_infos.size(), 2u);
  EXPECT_THAT(uploaded_infos,
              testing::UnorderedElementsAre(
                  testing::Field(&FileInfo::file_token, token1),
                  testing::Field(&FileInfo::file_token, tab_token1)));

  session_handle->GetUploadedContextTokensForTesting().push_back(token2);
  session_handle->GetUploadedContextTokensForTesting().push_back(tab_token2);
  uploaded_infos = session_handle->GetUploadedContextFileInfos();
  EXPECT_THAT(uploaded_infos,
              testing::UnorderedElementsAre(
                  testing::Field(&FileInfo::file_token, token1),
                  testing::Field(&FileInfo::file_token, token2),
                  testing::Field(&FileInfo::file_token, tab_token1),
                  testing::Field(&FileInfo::file_token, tab_token2)));

  // Test GetSubmittedContextFileInfos (after moving tokens).
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Invoke(
          this, &ContextualSearchServiceTest::CaptureClientToAimRequest));

  auto create_client_to_aim_request_info = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  session_handle->CreateClientToAimRequest(
      std::move(create_client_to_aim_request_info));

  std::vector<FileInfo> submitted_infos =
      session_handle->GetSubmittedContextFileInfos();
  EXPECT_THAT(submitted_infos,
              testing::UnorderedElementsAre(
                  testing::Field(&FileInfo::file_token, token1),
                  testing::Field(&FileInfo::file_token, token2),
                  testing::Field(&FileInfo::file_token, tab_token1),
                  testing::Field(&FileInfo::file_token, tab_token2)));

  // Also check that uploaded files is now empty.
  uploaded_infos = session_handle->GetUploadedContextFileInfos();
  EXPECT_TRUE(uploaded_infos.empty());

  // Add a new file to uploaded, and make sure submitted remains the same.
  base::UnguessableToken token3 = base::UnguessableToken::Create();
  FileInfo file_info3;
  file_info3.file_token = token3;
  file_info3.file_name = "file3.png";

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token3))
      .WillRepeatedly(testing::Return(&file_info3));

  session_handle->GetUploadedContextTokensForTesting().push_back(token3);

  uploaded_infos = session_handle->GetUploadedContextFileInfos();
  ASSERT_EQ(uploaded_infos.size(), 1u);
  EXPECT_EQ(uploaded_infos[0].file_token, token3);

  submitted_infos = session_handle->GetSubmittedContextFileInfos();
  EXPECT_THAT(submitted_infos,
              testing::UnorderedElementsAre(
                  testing::Field(&FileInfo::file_token, token1),
                  testing::Field(&FileInfo::file_token, token2),
                  testing::Field(&FileInfo::file_token, tab_token1),
                  testing::Field(&FileInfo::file_token, tab_token2)));

  // Test that a token with no FileInfo is ignored.
  base::UnguessableToken token4 = base::UnguessableToken::Create();
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token4))
      .WillRepeatedly(testing::Return(nullptr));
  session_handle->GetUploadedContextTokensForTesting().push_back(token4);
  uploaded_infos = session_handle->GetUploadedContextFileInfos();
  ASSERT_EQ(uploaded_infos.size(), 1u);
  EXPECT_EQ(uploaded_infos[0].file_token, token3);
}

TEST_F(ContextualSearchServiceTest, StartModalityChipUploadFlow) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
      ContextualSearchSource::kUnknown);

  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto session_handle = service_->CreateSessionForTesting(
      std::move(mock_controller), std::move(metrics_recorder));
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  base::UnguessableToken file_token = session_handle->CreateContextToken();

  auto modality_chip_props = std::make_unique<lens::ModalityChipProps>();
  modality_chip_props->set_id("test_chip_id");

  // Expect StartFileUploadFlow to be called with the modality chip props.
  EXPECT_CALL(*mock_controller_ptr,
              StartFileUploadFlow(file_token, testing::NotNull(), _))
      .WillOnce(
          testing::WithArgs<1>([&](std::unique_ptr<lens::ContextualInputData>
                                       contextual_input_data) {
            EXPECT_TRUE(contextual_input_data->modality_chip_props.has_value());
            EXPECT_EQ(contextual_input_data->modality_chip_props->id(),
                      "test_chip_id");
          }));

  session_handle->StartModalityChipUploadFlow(file_token,
                                              std::move(modality_chip_props));
}

TEST_F(ContextualSearchServiceTest, NullController) {
  // Create a session.
  auto config_params =
      std::make_unique<ContextualSearchContextController::ConfigParams>();
  auto session_handle = service_->CreateSession(
      std::move(config_params), ContextualSearchSource::kUnknown,
      /*invocation_source=*/std::nullopt);
  ASSERT_THAT(session_handle, NotNull());
  session_handle->CheckSearchContentSharingSettings(&pref_service_);
  session_handle->NotifySessionStarted();

  // Add some dummy tokens.
  session_handle->GetUploadedContextTokensForTesting().push_back(
      base::UnguessableToken::Create());
  auto create_client_to_aim_request_info = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  session_handle->CreateClientToAimRequest(
      std::move(create_client_to_aim_request_info));

  // Destroy the service. This will cause GetController() to return nullptr.
  service_.reset();

  // These calls should not crash.
  EXPECT_TRUE(session_handle->GetUploadedContextFileInfos().empty());
  EXPECT_TRUE(session_handle->GetSubmittedContextFileInfos().empty());
}

// Regression test for crbug.com/466564968
TEST_F(ContextualSearchServiceTest, CreateSessionAfterShutdown) {
  // Call Shutdown to clear the IdentityManager pointer.
  service_->Shutdown();

  // Destroy the IdentityManager to ensure the service is not holding a dangling
  // pointer.
  identity_test_env_.reset();

  // Create a new session. This should not crash even if IdentityManager is gone
  // (simulated by it being null after Shutdown).
  auto config_params =
      std::make_unique<ContextualSearchContextController::ConfigParams>();
  auto session_handle = service_->CreateSession(
      std::move(config_params), ContextualSearchSource::kUnknown,
      /*invocation_source=*/std::nullopt);

  ASSERT_THAT(session_handle, NotNull());
  ASSERT_THAT(session_handle->GetController(), NotNull());
}

TEST_F(ContextualSearchServiceTest, DeleteFile) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
      ContextualSearchSource::kUnknown);

  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto session_handle = service_->CreateSessionForTesting(
      std::move(mock_controller), std::move(metrics_recorder));
  // Check the search content sharing settings to notify the session handle
  // that the client is properly checking the pref value.
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  // Create a token.
  base::UnguessableToken token1 = session_handle->CreateContextToken();
  contextual_search::FileInfo file_info1;
  file_info1.file_token = token1;

  // Expect controller DeleteFile to be called.
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token1))
      .WillRepeatedly(testing::Return(&file_info1));
  EXPECT_CALL(*mock_controller_ptr, DeleteFile(token1))
      .WillOnce(testing::Return(true));

  // Case 1: Delete uploaded file.
  // The file has not been submitted yet, so it should be deleted.
  EXPECT_TRUE(session_handle->DeleteFile(token1));

  // Create another token.
  base::UnguessableToken token2 = session_handle->CreateContextToken();
  contextual_search::FileInfo file_info2;
  file_info2.file_token = token2;

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token2))
      .WillRepeatedly(testing::Return(&file_info2));

  // Submit the token.
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Invoke(
          this, &ContextualSearchServiceTest::CaptureClientToAimRequest));

  auto request = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  session_handle->CreateClientToAimRequest(std::move(request));

  // Token2 should now be in submitted tokens.
  // Verify DeleteFile is NOT called on controller.
  EXPECT_CALL(*mock_controller_ptr, DeleteFile(token2)).Times(0);

  // Case 2: Delete submitted file.
  // The file has been submitted, so it should NOT be deleted.
  EXPECT_FALSE(session_handle->DeleteFile(token2));
}

TEST_F(ContextualSearchServiceTest, StartFileContextUploadFlow_PdfPageTitle) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
      ContextualSearchSource::kUnknown);

  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto session_handle = service_->CreateSessionForTesting(
      std::move(mock_controller), std::move(metrics_recorder));
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  base::UnguessableToken file_token = session_handle->CreateContextToken();
  std::string file_name = "test_file.pdf";
  std::string file_mime_type = "application/pdf";
  std::vector<uint8_t> file_bytes = {1, 2, 3};
  mojo_base::BigBuffer buffer(file_bytes);

  // Expect StartFileUploadFlow to be called with the page title set to the file
  // name.
  EXPECT_CALL(*mock_controller_ptr,
              StartFileUploadFlow(file_token, testing::NotNull(), _))
      .WillOnce(
          testing::WithArgs<1>([&](std::unique_ptr<lens::ContextualInputData>
                                       contextual_input_data) {
            EXPECT_TRUE(contextual_input_data->page_title.has_value());
            EXPECT_EQ(contextual_input_data->page_title.value(), file_name);
            EXPECT_TRUE(contextual_input_data->file_name.has_value());
            EXPECT_EQ(contextual_input_data->file_name.value(), file_name);
          }));

  session_handle->StartFileContextUploadFlow(
      file_token, file_name, file_mime_type, std::move(buffer), std::nullopt);
}

TEST_F(ContextualSearchServiceTest,
       StartFileContextUploadFlow_ImageNoPageTitle) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
      ContextualSearchSource::kUnknown);

  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto session_handle = service_->CreateSessionForTesting(
      std::move(mock_controller), std::move(metrics_recorder));
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  base::UnguessableToken file_token = session_handle->CreateContextToken();
  std::string file_name = "test_image.png";
  std::string file_mime_type = "image/png";
  std::vector<uint8_t> file_bytes = {1, 2, 3};
  mojo_base::BigBuffer buffer(file_bytes);

  // Expect StartFileUploadFlow to be called without the page title set.
  EXPECT_CALL(*mock_controller_ptr,
              StartFileUploadFlow(file_token, testing::NotNull(), _))
      .WillOnce(
          testing::WithArgs<1>([&](std::unique_ptr<lens::ContextualInputData>
                                       contextual_input_data) {
            EXPECT_FALSE(contextual_input_data->page_title.has_value());
            EXPECT_TRUE(contextual_input_data->file_name.has_value());
            EXPECT_EQ(contextual_input_data->file_name.value(), file_name);
          }));

  session_handle->StartFileContextUploadFlow(
      file_token, file_name, file_mime_type, std::move(buffer), std::nullopt);
}

}  // namespace contextual_search
