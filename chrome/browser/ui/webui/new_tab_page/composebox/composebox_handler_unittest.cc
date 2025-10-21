// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/omnibox/contextual_session_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/contextual_session_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

using composebox::SessionState;

namespace {
constexpr char kClientUploadDurationQueryParameter[] = "cud";
constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";
constexpr char kQueryText[] = "query";
constexpr char kComposeboxFileDeleted[] =
    "NewTabPage.Composebox.Session.File.DeletedCount";

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
    ContextualSearchboxHandlerTestHarness::SetUp();

    auto query_controller_config_params = std::make_unique<
        ComposeboxQueryController::QueryControllerConfigParams>();
    query_controller_config_params->send_lns_surface = false;
    query_controller_config_params->enable_multi_context_input_flow = false;
    query_controller_config_params->enable_viewport_images = true;
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        version_info::Channel::UNKNOWN, "en-US", template_url_service(),
        fake_variations_client(), std::move(query_controller_config_params));
    query_controller_ = query_controller_ptr.get();

    service_ = std::make_unique<ContextualSessionService>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        template_url_service(), fake_variations_client(),
        version_info::Channel::UNKNOWN, "en-US");
    auto contextual_session_handle =
        service_->CreateSessionForTesting(std::move(query_controller_ptr));
    ContextualSessionWebContentsHelper::GetOrCreateForWebContents(
        web_contents())
        ->set_session_handle(std::move(contextual_session_handle));

    web_contents()->SetDelegate(&delegate_);
    auto metrics_recorder_ptr =
        std::make_unique<MockComposeboxMetricsRecorder>();
    metrics_recorder_ = metrics_recorder_ptr.get();
    handler_ = std::make_unique<ComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        std::move(metrics_recorder_ptr), profile(), web_contents());

    handler_->SetPage(mock_searchbox_page_.BindAndGetRemote());
  }

  ComposeboxHandler& handler() { return *handler_; }
  MockQueryController& query_controller() { return *query_controller_; }
  MockComposeboxMetricsRecorder& metrics_recorder() {
    return *metrics_recorder_;
  }

  void SubmitQueryAndWaitForNavigation() {
    content::TestNavigationObserver navigation_observer(web_contents());
    handler().SubmitQuery(kQueryText, 1, false, false, false, false);
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
    service_.reset();
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
  std::unique_ptr<ContextualSessionService> service_;
  raw_ptr<MockComposeboxMetricsRecorder> metrics_recorder_;
  std::unique_ptr<ComposeboxHandler> handler_;
};

TEST_F(ComposeboxHandlerTest, SetDeepSearchMode) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  query_controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  EXPECT_CALL(query_controller(), NotifySessionStarted)
      .Times(1)
      .WillOnce(testing::Invoke(
          &query_controller(), &MockQueryController::NotifySessionStartedBase));
  handler().NotifySessionStarted();
  run_loop.Run();

  // Submitting without setting deep search.
  std::string dr_param;
  SubmitQueryAndWaitForNavigation();
  GURL query_url =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  EXPECT_FALSE(net::GetValueForKeyInQuery(query_url, "dr", &dr_param));

  // Submitting with setting deep search.
  handler().SetDeepSearchMode(true);
  histogram_tester().ExpectUniqueSample(
      "NewTabPage.Composebox.Tools.DeepSearch",
      static_cast<int>(AimToolState::kEnabled), 1);
  SubmitQueryAndWaitForNavigation();
  GURL query_url_dr =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  EXPECT_TRUE(net::GetValueForKeyInQuery(query_url_dr, "dr", &dr_param));
  EXPECT_EQ("1", dr_param);

  // Submitting after disabling deep search.
  handler().SetDeepSearchMode(false);
  histogram_tester().ExpectTotalCount("NewTabPage.Composebox.Tools.DeepSearch",
                                      2);
  histogram_tester().ExpectBucketCount("NewTabPage.Composebox.Tools.DeepSearch",
                                       static_cast<int>(AimToolState::kEnabled),
                                       1);
  histogram_tester().ExpectBucketCount(
      "NewTabPage.Composebox.Tools.DeepSearch",
      static_cast<int>(AimToolState::kDisabled), 1);
  SubmitQueryAndWaitForNavigation();
  GURL query_url_disabled_dr =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  EXPECT_FALSE(
      net::GetValueForKeyInQuery(query_url_disabled_dr, "dr", &dr_param));
}

TEST_F(ComposeboxHandlerTest, SetCreateImageMode) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  query_controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  EXPECT_CALL(query_controller(), NotifySessionStarted)
      .Times(1)
      .WillOnce(testing::Invoke(
          &query_controller(), &MockQueryController::NotifySessionStartedBase));
  handler().NotifySessionStarted();
  run_loop.Run();

  // Submitting with create image mode enabled.
  handler().SetCreateImageMode(true, /*image_present= */ false);
  histogram_tester().ExpectUniqueSample(
      "NewTabPage.Composebox.Tools.CreateImage",
      static_cast<int>(AimToolState::kEnabled), 1);
  SubmitQueryAndWaitForNavigation();
  GURL query_url_create_image =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  std::string imgn_param;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(query_url_create_image, "imgn", &imgn_param));
  EXPECT_EQ("1", imgn_param);

  // Submitting with create image mode disabled.
  handler().SetCreateImageMode(false, /*image_present= */ false);
  histogram_tester().ExpectTotalCount("NewTabPage.Composebox.Tools.CreateImage",
                                      2);
  histogram_tester().ExpectBucketCount(
      "NewTabPage.Composebox.Tools.CreateImage",
      static_cast<int>(AimToolState::kEnabled), 1);
  histogram_tester().ExpectBucketCount(
      "NewTabPage.Composebox.Tools.CreateImage",
      static_cast<int>(AimToolState::kDisabled), 1);
  SubmitQueryAndWaitForNavigation();
  GURL query_url_disabled_create_image =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  EXPECT_FALSE(net::GetValueForKeyInQuery(query_url_disabled_create_image,
                                          "imgn", &imgn_param));
}

TEST_F(ComposeboxHandlerTest, DeleteFileAndSubmitQuery) {
  std::string file_type = ".Image";
  std::string file_status = ".NotUploaded";
  std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  file_info->file_name = "test.png";
  file_info->mime_type_ = lens::MimeType::kImage;
  base::UnguessableToken delete_file_token = base::UnguessableToken::Create();
  base::UnguessableToken token_arg;
  EXPECT_CALL(query_controller(), DeleteFile)
      .WillOnce([&token_arg](const base::UnguessableToken& token) {
        token_arg = token;
        return true;
      });

  EXPECT_CALL(query_controller(), GetFileInfo)
      .WillOnce([&file_info](const base::UnguessableToken& token) {
        return file_info.get();
      });

  handler().DeleteContext(delete_file_token);

  SubmitQueryAndWaitForNavigation();

  EXPECT_EQ(delete_file_token, token_arg);
  histogram_tester().ExpectTotalCount(
      kComposeboxFileDeleted + file_type + file_status, 1);
}

TEST_F(ComposeboxHandlerTest, SubmitQueryWithToolMetric) {
  // Submit with no tools enabled.
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectUniqueSample(
      "NewTabPage.Composebox.Tools.SubmissionType",
      static_cast<int>(SubmissionType::kDefault), 1);

  // Submitting with deep search mode enabled.
  handler().SetDeepSearchMode(true);
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectBucketCount(
      "NewTabPage.Composebox.Tools.SubmissionType",
      static_cast<int>(SubmissionType::kDeepSearch), 1);

  // Submitting with create image mode enabled.
  handler().SetCreateImageMode(true, /*image_present= */ false);
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectBucketCount(
      "NewTabPage.Composebox.Tools.SubmissionType",
      static_cast<int>(SubmissionType::kCreateImages), 1);

  histogram_tester().ExpectTotalCount(
      "NewTabPage.Composebox.Tools.SubmissionType", 3);
}
