// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

namespace {
constexpr char kClientUploadDurationQueryParameter[] = "cud";
constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";
constexpr char kQueryText[] = "query";
constexpr char kComposeboxFileDeleted[] =
    "ContextualSearch.Session.File.DeletedCount";

class MockPage : public composebox::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<composebox::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  mojo::Receiver<composebox::mojom::Page> receiver_{this};
};

}  // namespace

class ComposeboxHandlerTest : public ContextualSearchboxHandlerTestHarness {
 public:
  ~ComposeboxHandlerTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(ContextualSearchboxHandlerTestHarness::SetUp());
    auto query_controller_config_params = std::make_unique<
        contextual_search::ContextualSearchContextController::ConfigParams>();
    query_controller_config_params->send_lns_surface = false;
    query_controller_config_params->enable_viewport_images = true;
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        version_info::Channel::UNKNOWN, "en-US", template_url_service(),
        /*variations_client=*/nullptr,
        std::move(query_controller_config_params));
    query_controller_ = query_controller_ptr.get();

    auto metrics_recorder_ptr =
        std::make_unique<MockContextualSearchMetricsRecorder>();
    metrics_recorder_ = metrics_recorder_ptr.get();
    ON_CALL(*metrics_recorder_, RecordModesOnSubmission)
        .WillByDefault(testing::Invoke(
            metrics_recorder_.get(),
            &MockContextualSearchMetricsRecorder::RecordModesOnSubmissionBase));

    service_ = ContextualSearchServiceFactory::GetForProfile(profile());
    contextual_session_handle_ = service_->CreateSessionForTesting(
        std::move(query_controller_ptr), std::move(metrics_recorder_ptr));
    // Check the search content sharing settings to notify the session handle
    // that the client is properly checking the pref value.
    contextual_session_handle_->CheckSearchContentSharingSettings(
        profile()->GetPrefs());

    web_contents()->SetDelegate(&delegate_);
    handler_ = std::make_unique<ComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        mock_searchbox_page_.BindAndGetRemote(), profile(), web_contents(),
        base::BindLambdaForTesting(
            [&]() { return contextual_session_handle_.get(); }),
        base::DoNothing());
  }

  ComposeboxHandler& handler() { return *handler_; }
  MockQueryController& query_controller() { return *query_controller_; }
  MockContextualSearchMetricsRecorder& metrics_recorder() {
    return *metrics_recorder_;
  }
  contextual_search::ContextualSearchSessionHandle*
  contextual_session_handle() {
    return contextual_session_handle_.get();
  }

  void SubmitQueryAndWaitForNavigation() {
    content::TestNavigationObserver navigation_observer(web_contents());
    handler().SubmitQuery(kQueryText, 1, false, false, false, false,
                          /*is_voice_search=*/false);
    auto navigation = content::NavigationSimulator::CreateFromPending(
        web_contents()->GetController());
    ASSERT_TRUE(navigation);
    navigation->Commit();
    navigation_observer.Wait();
  }

  void TearDown() override {
    query_controller_ = nullptr;
    metrics_recorder_ = nullptr;
    handler_.reset();
    contextual_session_handle_.reset();
    // Service is owned by the profile, so we don't need to reset it here,
    // but we should clear the pointer.
    service_ = nullptr;
    ContextualSearchboxHandlerTestHarness::TearDown();
  }

  GURL StripTimestampsFromAimUrl(const GURL& url) {
    std::string qsubts_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(
        url, kQuerySubmissionTimeQueryParameter, &qsubts_param));

    std::string cud_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(
        url, kClientUploadDurationQueryParameter, &cud_param));

    GURL result_url = url;
    result_url = net::AppendOrReplaceQueryParameter(
        result_url, kQuerySubmissionTimeQueryParameter, std::nullopt);
    result_url = net::AppendOrReplaceQueryParameter(
        result_url, kClientUploadDurationQueryParameter, std::nullopt);
    return result_url;
  }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  testing::NiceMock<MockSearchboxPage> mock_searchbox_page_;

 private:
  TestWebContentsDelegate delegate_;
  raw_ptr<MockQueryController> query_controller_;
  raw_ptr<contextual_search::ContextualSearchService> service_;
  raw_ptr<MockContextualSearchMetricsRecorder> metrics_recorder_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      contextual_session_handle_;
  std::unique_ptr<ComposeboxHandler> handler_;
};

TEST_F(ComposeboxHandlerTest, DeleteFileAndSubmitQuery) {
  std::string file_type = ".Image";
  std::string file_status = ".NotUploaded";
  std::unique_ptr<contextual_search::FileInfo> file_info =
      std::make_unique<contextual_search::FileInfo>();
  file_info->file_name = "test.png";
  file_info->mime_type = lens::MimeType::kImage;
  file_info->upload_status =
      contextual_search::ContextUploadStatus::kNotUploaded;
  file_info->tab_session_id = SessionID::FromSerializedValue(123);
  base::UnguessableToken delete_file_token = base::UnguessableToken::Create();
  base::UnguessableToken token_arg;
  EXPECT_CALL(query_controller(), GetFileInfo(delete_file_token))
      .Times(1)
      .WillRepeatedly(testing::Return(file_info.get()));
  EXPECT_CALL(query_controller(), DeleteFile(delete_file_token))
      .WillOnce([&token_arg](const base::UnguessableToken& token) {
        token_arg = token;
        return true;
      });

  handler().DeleteContext(delete_file_token, /*from_automatic_chip=*/false);

  SubmitQueryAndWaitForNavigation();

  EXPECT_EQ(delete_file_token, token_arg);
  histogram_tester().ExpectTotalCount(
      kComposeboxFileDeleted + file_type + file_status + ".NewTabPage", 1);
}

// Verifies that TakeSessionHandle transfers ownership out of the helper.
TEST_F(ComposeboxHandlerTest, TakeSessionHandle_TransfersOwnership) {
  auto mock_controller = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchContextController>>();
  ON_CALL(*mock_controller, AsWeakPtr())
      .WillByDefault(testing::Return(
          base::WeakPtr<
              contextual_search::ContextualSearchContextController>()));

  auto* service = ContextualSearchServiceFactory::GetForProfile(profile());
  auto handle = service->CreateSessionForTesting(std::move(mock_controller),
                                                 /*metrics_recorder=*/nullptr);

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents());
  helper->SetTaskSession(/*task_id=*/std::nullopt, std::move(handle),
                         /*input_state_model=*/nullptr);
  EXPECT_NE(helper->session_handle(), nullptr);

  auto taken_handle = helper->TakeSessionHandle();
  EXPECT_NE(taken_handle, nullptr);
  EXPECT_EQ(helper->session_handle(), nullptr);
}

TEST_F(ComposeboxHandlerTest, SubmitQueryWithToolMetric) {
  // Submit with no tools enabled.
  EXPECT_CALL(metrics_recorder(),
              RecordModesOnSubmission(
                  omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
                  omnibox::ModelMode::MODEL_MODE_UNSPECIFIED, testing::_))
      .Times(1);
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.ModeOnSubmission.NewTabPage",
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, 1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Models.ModeOnSubmission.NewTabPage",
      omnibox::ModelMode::MODEL_MODE_UNSPECIFIED, 1);

  // Submitting with deep search and Gemini regular model enabled.
  handler().SetActiveToolMode(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  handler().SetActiveToolMode(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  handler().RecordToolSelectionAction(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  handler().SetActiveModelMode(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  handler().RecordModelSelectionAction(
      omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  EXPECT_CALL(metrics_recorder(),
              RecordModesOnSubmission(
                  omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                  omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR, testing::_))
      .Times(1);
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.ModeOnSubmission.NewTabPage",
      omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH, 1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Models.ModeOnSubmission.NewTabPage",
      omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR, 1);

  // Submitting with create image and Gemini Pro model enabled.
  handler().SetActiveToolMode(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  handler().RecordToolSelectionAction(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  handler().SetActiveModelMode(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  handler().RecordModelSelectionAction(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  EXPECT_CALL(metrics_recorder(),
              RecordModesOnSubmission(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
                                      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO,
                                      testing::_))
      .Times(1);
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.ModeOnSubmission.NewTabPage",
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN, 1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Models.ModeOnSubmission.NewTabPage",
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO, 1);

  histogram_tester().ExpectTotalCount(
      "ContextualSearch.Tools.ModeOnSubmission.NewTabPage", 3);
  histogram_tester().ExpectTotalCount(
      "ContextualSearch.Models.ModeOnSubmission.NewTabPage", 3);
}

TEST_F(ComposeboxHandlerTest, SetSmartTabSharingActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      contextual_tasks::kContextualTasksContext,
      {{"ContextualTasksContextSmartTabSharing", "true"}});

  EXPECT_FALSE(handler().IsSmartTabSharingActive());

  handler().SetSmartTabSharingActive(true);
  EXPECT_TRUE(handler().IsSmartTabSharingActive());

  handler().SetSmartTabSharingActive(false);
  EXPECT_FALSE(handler().IsSmartTabSharingActive());
}

TEST_F(ComposeboxHandlerTest, OnContextMenuOpenedTriggersFetch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      contextual_tasks::kContextualTasksLazyFetchClusterInfo);

  EXPECT_CALL(query_controller(), TriggerFetchClusterInfo());
  handler().OnContextMenuOpened();
}

// Verifies that when views deletes context, it notifies the webUI to update.
TEST_F(ComposeboxHandlerTest, DeleteContext_NotifiesPage) {
  base::UnguessableToken delete_file_token = base::UnguessableToken::Create();

  std::unique_ptr<contextual_search::FileInfo> file_info =
      std::make_unique<contextual_search::FileInfo>();
  file_info->file_name = "test.png";
  file_info->mime_type = lens::MimeType::kImage;
  file_info->upload_status =
      contextual_search::ContextUploadStatus::kNotUploaded;

  EXPECT_CALL(query_controller(), GetFileInfo(delete_file_token))
      .WillRepeatedly(testing::Return(file_info.get()));
  EXPECT_CALL(query_controller(), DeleteFile(delete_file_token))
      .WillOnce(testing::Return(true));

  // Verify that C++ notifies WebUI page with kUploadReplaced status.
  EXPECT_CALL(mock_searchbox_page_,
              OnContextualInputStatusChanged(
                  delete_file_token,
                  contextual_search::ContextUploadStatus::kUploadReplaced,
                  std::optional<contextual_search::ContextUploadErrorType>()))
      .Times(1);

  handler().DeleteContextFromBrowser(delete_file_token,
                                     /*from_automatic_chip=*/false);
  mock_searchbox_page_.FlushForTesting();
}

// Verifies that when views does not delete context,
// it does not notify the webUI to update.
TEST_F(ComposeboxHandlerTest, DeleteContext_MojoDoesNotNotifyPage) {
  base::UnguessableToken delete_file_token = base::UnguessableToken::Create();

  std::unique_ptr<contextual_search::FileInfo> file_info =
      std::make_unique<contextual_search::FileInfo>();
  file_info->file_name = "test.png";
  file_info->mime_type = lens::MimeType::kImage;
  file_info->upload_status =
      contextual_search::ContextUploadStatus::kNotUploaded;

  EXPECT_CALL(query_controller(), GetFileInfo(delete_file_token))
      .WillRepeatedly(testing::Return(file_info.get()));
  EXPECT_CALL(query_controller(), DeleteFile(delete_file_token))
      .WillOnce(testing::Return(true));

  // Verify that C++ does NOT notify WebUI page.
  EXPECT_CALL(mock_searchbox_page_, OnContextualInputStatusChanged).Times(0);

  handler().DeleteContext(delete_file_token, /*from_automatic_chip=*/false);
  mock_searchbox_page_.FlushForTesting();
}
