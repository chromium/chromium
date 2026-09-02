// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
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
#include "chrome/common/pref_names.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_controller.h"
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
#include "ui/base/window_open_disposition.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

namespace {
constexpr char kClientUploadDurationQueryParameter[] = "cud";
constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";
constexpr char kQueryText[] = "query";
constexpr char kComposeboxFileDeleted[] =
    "ContextualSearch.Session.File.DeletedCount";


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

    ON_CALL(*query_controller_, GetFileInfo)
        .WillByDefault(testing::Invoke(query_controller_.get(),
                                       &MockQueryController::FakeGetFileInfo));

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
    base::RunLoop().RunUntilIdle();
    auto navigation = content::NavigationSimulator::CreateFromPending(
        web_contents()->GetController());
    ASSERT_TRUE(navigation);
    auto callback = delegate_.TakeCallback();
    if (callback) {
      std::move(callback).Run(*(navigation->GetNavigationHandle()));
    }
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
    delegate_.ClearCallback();
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
  handler().SetActiveToolMode(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                              /*is_set_by_aim=*/false);
  handler().SetActiveToolMode(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                              /*is_set_by_aim=*/false);
  handler().RecordToolSelectionAction(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  handler().SetActiveModelMode(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR,
                               /*is_set_by_server=*/false);
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
  handler().SetActiveToolMode(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
                              /*is_set_by_aim=*/false);
  handler().RecordToolSelectionAction(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  handler().SetActiveModelMode(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO,
                               /*is_set_by_server=*/false);
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
  feature_list.InitWithFeaturesAndParameters(
      {{contextual_tasks::kContextualTasksContext,
        {{"ContextualTasksContextSmartTabSharing", "true"}}},
       {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}}},
      {});

  EXPECT_FALSE(handler().IsSmartTabSharingActive());

  handler().SetSmartTabSharingActive(true);
  EXPECT_TRUE(handler().IsSmartTabSharingActive());

  handler().SetSmartTabSharingActive(false);
  EXPECT_FALSE(handler().IsSmartTabSharingActive());
}

TEST_F(ComposeboxHandlerTest, ResetInputStateModelClearsSmartTabSharingActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{contextual_tasks::kContextualTasksContext,
        {{"ContextualTasksContextSmartTabSharing", "true"}}},
       {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}}},
      {});

  EXPECT_FALSE(handler().IsSmartTabSharingActive());

  handler().SetSmartTabSharingActive(true);
  EXPECT_TRUE(handler().IsSmartTabSharingActive());

  // Starting a new session resets the session handle and input state model.
  contextual_session_handle()->set_smart_tab_sharing_active(std::nullopt);
  handler().ResetInputStateModel();
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

TEST_F(ComposeboxHandlerTest, NextboxAnimationLimiting) {
  base::HistogramTester histogram_tester;
  PrefService* prefs = profile()->GetPrefs();

  // 1. Initially allowed, counts are 0.
  {
    base::test::TestFuture<bool> future;
    handler().CanShowNextboxAnimation(future.GetCallback());
    EXPECT_TRUE(future.Take());

    const base::DictValue& dict =
        prefs->GetDict(prefs::kContextMenuAnimationState);
    EXPECT_EQ(std::nullopt, dict.FindInt("nextbox_daily_count"));
    EXPECT_EQ(std::nullopt, dict.FindInt("nextbox_lifetime_count"));
  }

  // 2. Record 1st impression.
  {
    handler().RecordNextboxAnimationImpression(/*shown=*/true);

    const base::DictValue& dict =
        prefs->GetDict(prefs::kContextMenuAnimationState);
    EXPECT_THAT(dict.FindInt("nextbox_daily_count"), testing::Optional(1));
    EXPECT_THAT(dict.FindInt("nextbox_lifetime_count"), testing::Optional(1));
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextMenu.AnimationShown.ContextualTasks", true, 1);
  }

  // 3. Play 4 more times (total 5 daily impressions recorded).
  for (int i = 0; i < 4; ++i) {
    base::test::TestFuture<bool> future;
    handler().CanShowNextboxAnimation(future.GetCallback());
    EXPECT_TRUE(future.Take());
    handler().RecordNextboxAnimationImpression(/*shown=*/true);
  }

  // Verify counts are now 5 daily and 5 lifetime.
  {
    const base::DictValue& dict =
        prefs->GetDict(prefs::kContextMenuAnimationState);
    EXPECT_THAT(dict.FindInt("nextbox_daily_count"), testing::Optional(5));
    EXPECT_THAT(dict.FindInt("nextbox_lifetime_count"), testing::Optional(5));
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextMenu.AnimationShown.ContextualTasks", true, 5);
  }

  // 4. The 6th time, it should not be allowed and record should do nothing to
  // prefs.
  {
    base::test::TestFuture<bool> future;
    handler().CanShowNextboxAnimation(future.GetCallback());
    EXPECT_FALSE(future.Take());

    handler().RecordNextboxAnimationImpression(/*shown=*/false);

    const base::DictValue& dict =
        prefs->GetDict(prefs::kContextMenuAnimationState);
    EXPECT_THAT(dict.FindInt("nextbox_daily_count"), testing::Optional(5));
    EXPECT_THAT(dict.FindInt("nextbox_lifetime_count"), testing::Optional(5));
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextMenu.AnimationShown.ContextualTasks", false, 1);
  }

  // 5. Simulate a new day (change the date string in prefs).
  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                prefs::kContextMenuAnimationState);
    update->Set("nextbox_last_impression_time",
                base::TimeToValue(base::Time::Now() - base::Days(1)));
  }

  // 6. Requesting now should reset daily count and allow more impressions.
  {
    base::test::TestFuture<bool> future;
    handler().CanShowNextboxAnimation(future.GetCallback());
    EXPECT_TRUE(future.Take());

    handler().RecordNextboxAnimationImpression(/*shown=*/true);

    const base::DictValue& dict =
        prefs->GetDict(prefs::kContextMenuAnimationState);
    EXPECT_THAT(dict.FindInt("nextbox_daily_count"), testing::Optional(1));
    EXPECT_THAT(dict.FindInt("nextbox_lifetime_count"), testing::Optional(6));
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextMenu.AnimationShown.ContextualTasks", true, 6);
  }

  // 7. Bring lifetime count to 19 and verify it caps after 20.
  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                prefs::kContextMenuAnimationState);
    update->Set("nextbox_lifetime_count", 19);
    update->Set("nextbox_daily_count",
                0);  // Reset daily for today so we don't hit daily cap.
  }

  // 20th lifetime impression should still play.
  {
    base::test::TestFuture<bool> future;
    handler().CanShowNextboxAnimation(future.GetCallback());
    EXPECT_TRUE(future.Take());

    handler().RecordNextboxAnimationImpression(/*shown=*/true);

    const base::DictValue& dict =
        prefs->GetDict(prefs::kContextMenuAnimationState);
    EXPECT_THAT(dict.FindInt("nextbox_daily_count"), testing::Optional(1));
    EXPECT_THAT(dict.FindInt("nextbox_lifetime_count"), testing::Optional(20));
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextMenu.AnimationShown.ContextualTasks", true, 7);
  }

  // 21st lifetime impression should be blocked.
  {
    base::test::TestFuture<bool> future;
    handler().CanShowNextboxAnimation(future.GetCallback());
    EXPECT_FALSE(future.Take());

    handler().RecordNextboxAnimationImpression(/*shown=*/false);

    const base::DictValue& dict =
        prefs->GetDict(prefs::kContextMenuAnimationState);
    EXPECT_THAT(dict.FindInt("nextbox_daily_count"), testing::Optional(1));
    EXPECT_THAT(dict.FindInt("nextbox_lifetime_count"), testing::Optional(20));
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextMenu.AnimationShown.ContextualTasks", false, 2);
  }
}

class DestructingTestWebContentsDelegate : public TestWebContentsDelegate {
 public:
  explicit DestructingTestWebContentsDelegate(base::OnceClosure on_open_url)
      : on_open_url_(std::move(on_open_url)) {}

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    if (on_open_url_) {
      std::move(on_open_url_).Run();
    }
    return TestWebContentsDelegate::OpenURLFromTab(
        source, params, std::move(navigation_handle_callback));
  }

 private:
  base::OnceClosure on_open_url_;
};

TEST_F(ComposeboxHandlerTest, ProcessContextAndOpenUrl_DestructionSafe) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kOmniboxEverywhere);

  class TestComposeboxHandler : public ComposeboxHandler {
   public:
    using ComposeboxHandler::ComposeboxHandler;

    void ProcessContextAndOpenUrl(
        GURL url,
        const WindowOpenDisposition disposition) override {
      auto weak_this = weak_ptr_factory_.GetWeakPtr();
      ContextualSearchboxHandler::ProcessContextAndOpenUrl(url, disposition);
      if (!weak_this) {
        return;
      }
      ResetInputStateModel();
      ClearSessionHandle();
      InitializeInputStateModel();
      auto* contextual_session_handle = GetContextualSessionHandle();
      if (contextual_session_handle) {
        contextual_session_handle->NotifySessionStarted();
      }
    }

   private:
    base::WeakPtrFactory<TestComposeboxHandler> weak_ptr_factory_{this};
  };

  std::unique_ptr<TestComposeboxHandler> test_handler;
  DestructingTestWebContentsDelegate destructing_delegate(
      base::BindLambdaForTesting([&]() { test_handler.reset(); }));
  web_contents()->SetDelegate(&destructing_delegate);

  mock_searchbox_page_.receiver_.reset();

  test_handler = std::make_unique<TestComposeboxHandler>(
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      mock_searchbox_page_.BindAndGetRemote(), profile(), web_contents(),
      base::BindLambdaForTesting([&]() { return contextual_session_handle(); }),
      base::DoNothing());

  // Calling ProcessContextAndOpenUrl will post a navigation task to the task
  // runner. Running pending tasks will trigger
  // WebContentsDelegate::OpenURLFromTab, which synchronously destroys the
  // handler, and it should complete safely without any use-after-free crashes.
  test_handler->ProcessContextAndOpenUrl(GURL("https://google.com"),
                                         WindowOpenDisposition::CURRENT_TAB);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_handler, nullptr);
}

TEST_F(ComposeboxHandlerTest, SubmitQuery_DestructionSafe) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kOmniboxEverywhere);

  class TestComposeboxHandler : public ComposeboxHandler {
   public:
    using ComposeboxHandler::ComposeboxHandler;

    void ProcessContextAndOpenUrl(
        GURL url,
        const WindowOpenDisposition disposition) override {
      auto weak_this = weak_ptr_factory_.GetWeakPtr();
      ContextualSearchboxHandler::ProcessContextAndOpenUrl(url, disposition);
      if (!weak_this) {
        return;
      }
      ResetInputStateModel();
      ClearSessionHandle();
      InitializeInputStateModel();
      auto* contextual_session_handle = GetContextualSessionHandle();
      if (contextual_session_handle) {
        contextual_session_handle->NotifySessionStarted();
      }
    }

   private:
    base::WeakPtrFactory<TestComposeboxHandler> weak_ptr_factory_{this};
  };

  std::unique_ptr<TestComposeboxHandler> test_handler;
  DestructingTestWebContentsDelegate destructing_delegate(
      base::BindLambdaForTesting([&]() {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindLambdaForTesting([&]() { test_handler.reset(); }));
      }));
  web_contents()->SetDelegate(&destructing_delegate);

  mock_searchbox_page_.receiver_.reset();

  test_handler = std::make_unique<TestComposeboxHandler>(
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      mock_searchbox_page_.BindAndGetRemote(), profile(), web_contents(),
      base::BindLambdaForTesting([&]() { return contextual_session_handle(); }),
      base::DoNothing());

  // SubmitQuery triggers ContextualizeQueryAndOpenUrl, which synchronously runs
  // the callback. The callback calls ComputeAndOpenQueryUrl, which calls
  // CreateSearchUrl, executing its callback synchronously. The callback
  // calls OpenUrl, which posts the navigation task. Running the posted task
  // will trigger WebContentsDelegate::OpenURLFromTab, destroying the handler.
  // All execution should complete safely without use-after-free crashes.
  test_handler->SubmitQuery("test query", 1, false, false, false, false, false);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_handler, nullptr);
}

TEST_F(ComposeboxHandlerTest, SubmitQuery_NullInputStateModel) {
  mock_searchbox_page_.receiver_.reset();

  auto test_handler = std::make_unique<ComposeboxHandler>(
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      mock_searchbox_page_.BindAndGetRemote(), profile(), web_contents(),
      base::BindLambdaForTesting(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }),
      base::DoNothing());

  // This should not crash and should return early.
  test_handler->SubmitQuery("test query", 1, false, false, false, false, false);
}

TEST_F(ComposeboxHandlerTest,
       ShouldOpenInLensSidePanel_ContextualTasksSidePanelEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::kContextualTasksSidePanel},
      /*disabled_features=*/{contextual_tasks::kContextualTasks});

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::UnguessableToken token = base::UnguessableToken::Create();
  query_controller().AddTabFileInfoForTesting(
      token, GURL("https://example.com"), lens::MimeType::kAnnotatedPageContent,
      tab_id);
  contextual_session_handle()->set_submitted_context_tokens({token});

  EXPECT_TRUE(handler().ShouldOpenInLensSidePanelForTesting(
      web_contents(), contextual_session_handle()));
}

TEST_F(
    ComposeboxHandlerTest,
    ShouldOpenInLensSidePanel_ContextualTasksSidePanelEnabled_CobrowseEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::kContextualTasksSidePanel,
                            contextual_tasks::
                                kContextualTasksForceEntryPointEligibility},
      /*disabled_features=*/{contextual_tasks::kContextualTasks});

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::UnguessableToken token = base::UnguessableToken::Create();
  query_controller().AddTabFileInfoForTesting(
      token, GURL("https://example.com"), lens::MimeType::kAnnotatedPageContent,
      tab_id);
  contextual_session_handle()->set_submitted_context_tokens({token});

  // When kContextualTasks (cobrowse) is disabled, even if the user is cobrowse
  // eligible, the query should route to the ContextualTasks side panel if
  // kContextualTasksSidePanel is enabled.
  EXPECT_TRUE(handler().ShouldOpenInLensSidePanelForTesting(
      web_contents(), contextual_session_handle()));
}

TEST_F(ComposeboxHandlerTest,
       ShouldOpenInLensSidePanel_ContextualTasksCobrowseEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::kContextualTasks,
                            contextual_tasks::
                                kContextualTasksForceEntryPointEligibility},
      /*disabled_features=*/{});

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::UnguessableToken token = base::UnguessableToken::Create();
  query_controller().AddTabFileInfoForTesting(
      token, GURL("https://example.com"), lens::MimeType::kAnnotatedPageContent,
      tab_id);
  contextual_session_handle()->set_submitted_context_tokens({token});

  // When kContextualTasks is enabled and eligible, cobrowse should handle
  // the navigation, so ShouldOpenInLensSidePanel returns false.
  EXPECT_FALSE(handler().ShouldOpenInLensSidePanelForTesting(
      web_contents(), contextual_session_handle()));
}

TEST_F(ComposeboxHandlerTest,
       ShouldOpenInLensSidePanel_ContextualTasksEnabled_CobrowseIneligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::kContextualTasks,
                            contextual_tasks::kContextualTasksSidePanel},
      /*disabled_features=*/{
          contextual_tasks::kContextualTasksForceEntryPointEligibility});

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::UnguessableToken token = base::UnguessableToken::Create();
  query_controller().AddTabFileInfoForTesting(
      token, GURL("https://example.com"), lens::MimeType::kAnnotatedPageContent,
      tab_id);
  contextual_session_handle()->set_submitted_context_tokens({token});

  // When kContextualTasks is enabled but the profile is ineligible for
  // cobrowse, it should route to the side panel if ContextualTasksSidePanel
  // is enabled.
  EXPECT_TRUE(handler().ShouldOpenInLensSidePanelForTesting(
      web_contents(), contextual_session_handle()));
}

TEST_F(ComposeboxHandlerTest, ShouldOpenInLensSidePanel_MultipleTabsAttached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::kContextualTasksSidePanel},
      /*disabled_features=*/{contextual_tasks::kContextualTasks});

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  query_controller().AddTabFileInfoForTesting(
      token1, GURL("https://example1.com"),
      lens::MimeType::kAnnotatedPageContent, tab_id);
  query_controller().AddTabFileInfoForTesting(
      token2, GURL("https://example2.com"),
      lens::MimeType::kAnnotatedPageContent,
      SessionID::FromSerializedValue(tab_id.id() + 1));
  contextual_session_handle()->set_submitted_context_tokens({token1, token2});

  // When multiple tabs are attached, fulfillment goes through the navigation
  // flow and is intercepted by ContextualTasksNavigationThrottle into the
  // side panel with all tabs, so ShouldOpenInLensSidePanel returns false.
  EXPECT_FALSE(handler().ShouldOpenInLensSidePanelForTesting(
      web_contents(), contextual_session_handle()));
}

TEST_F(
    ComposeboxHandlerTest,
    ShouldOpenInLensSidePanel_ContextualTasksCobrowseEligible_MultipleTabsAttached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::kContextualTasks,
                            contextual_tasks::
                                kContextualTasksForceEntryPointEligibility},
      /*disabled_features=*/{});

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  query_controller().AddTabFileInfoForTesting(
      token1, GURL("https://example1.com"),
      lens::MimeType::kAnnotatedPageContent, tab_id);
  query_controller().AddTabFileInfoForTesting(
      token2, GURL("https://example2.com"),
      lens::MimeType::kAnnotatedPageContent,
      SessionID::FromSerializedValue(tab_id.id() + 1));
  contextual_session_handle()->set_submitted_context_tokens({token1, token2});

  // When kContextualTasks (cobrowse) is enabled and eligible, cobrowse handles
  // navigation for multiple tabs as well, so ShouldOpenInLensSidePanel returns
  // false.
  EXPECT_FALSE(handler().ShouldOpenInLensSidePanelForTesting(
      web_contents(), contextual_session_handle()));
}

TEST_F(ComposeboxHandlerTest,
       ShouldOpenInLensSidePanel_MultipleTabsAttached_CurrentTabNotInContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::kContextualTasksSidePanel},
      /*disabled_features=*/{contextual_tasks::kContextualTasks});

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  query_controller().AddTabFileInfoForTesting(
      token1, GURL("https://example1.com"),
      lens::MimeType::kAnnotatedPageContent,
      SessionID::FromSerializedValue(tab_id.id() + 1));
  query_controller().AddTabFileInfoForTesting(
      token2, GURL("https://example2.com"),
      lens::MimeType::kAnnotatedPageContent,
      SessionID::FromSerializedValue(tab_id.id() + 2));
  contextual_session_handle()->set_submitted_context_tokens({token1, token2});

  EXPECT_FALSE(handler().ShouldOpenInLensSidePanelForTesting(
      web_contents(), contextual_session_handle()));
}

TEST_F(
    ComposeboxHandlerTest,
    ShouldOpenInLensSidePanel_ContextualTasksDisabled_MultipleTabsAttached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          contextual_tasks::kContextualTasksSidePanel,
          contextual_tasks::kContextualTasks,
          contextual_tasks::kContextualTasksRearchitecture});

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  query_controller().AddTabFileInfoForTesting(
      token1, GURL("https://example1.com"),
      lens::MimeType::kAnnotatedPageContent, tab_id);
  query_controller().AddTabFileInfoForTesting(
      token2, GURL("https://example2.com"),
      lens::MimeType::kAnnotatedPageContent,
      SessionID::FromSerializedValue(tab_id.id() + 1));
  contextual_session_handle()->set_submitted_context_tokens({token1, token2});

  // When contextual tasks is disabled, old Lens fallback behavior does not
  // support multiple context tokens and should return false.
  EXPECT_FALSE(handler().ShouldOpenInLensSidePanelForTesting(
      web_contents(), contextual_session_handle()));
}
