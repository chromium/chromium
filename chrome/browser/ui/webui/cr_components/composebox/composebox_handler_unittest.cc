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
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "components/contextual_search/contextual_search_service.h"
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

class TestEmbedder final : public TopChromeWebUIController::Embedder {
 public:
  TestEmbedder() = default;
  ~TestEmbedder() = default;

  void ShowUI() override {}
  void CloseUI() override {}
  void HideContextMenu() override {}

  void ShowContextMenu(gfx::Point point,
                       std::unique_ptr<ui::MenuModel> menu_model) override {
    context_menu_shown_ = true;
  }

  bool context_menu_shown() const { return context_menu_shown_; }

  base::WeakPtr<TestEmbedder> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool context_menu_shown_;

  base::WeakPtrFactory<TestEmbedder> weak_factory_{this};
};

}  // namespace

class ComposeboxHandlerTest : public ContextualSearchboxHandlerTestHarness {
 public:
  ~ComposeboxHandlerTest() override = default;

  void SetUp() override {
    ContextualSearchboxHandlerTestHarness::SetUp();

    auto query_controller_config_params = std::make_unique<
        contextual_search::ContextualSearchContextController::ConfigParams>();
    query_controller_config_params->send_lns_surface = false;
    query_controller_config_params->enable_multi_context_input_flow = false;
    query_controller_config_params->enable_viewport_images = true;
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        version_info::Channel::UNKNOWN, "en-US", template_url_service(),
        fake_variations_client(), std::move(query_controller_config_params));
    query_controller_ = query_controller_ptr.get();

    auto metrics_recorder_ptr =
        std::make_unique<MockContextualSearchMetricsRecorder>();
    metrics_recorder_ = metrics_recorder_ptr.get();

    service_ = std::make_unique<contextual_search::ContextualSearchService>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        template_url_service(), fake_variations_client(),
        version_info::Channel::UNKNOWN, "en-US");
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
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
        web_contents(), base::BindLambdaForTesting([&]() {
          return contextual_session_handle_.get();
        }));

    handler_->SetPage(mock_searchbox_page_.BindAndGetRemote());
    embedder_ = std::make_unique<TestEmbedder>();
    handler_->SetEmbedder(embedder_->GetWeakPtr());
  }

  ComposeboxHandler& handler() { return *handler_; }
  MockQueryController& query_controller() { return *query_controller_; }
  MockContextualSearchMetricsRecorder& metrics_recorder() {
    return *metrics_recorder_;
  }
  TestEmbedder& embedder() { return *embedder_; }

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
  std::unique_ptr<contextual_search::ContextualSearchService> service_;
  raw_ptr<MockContextualSearchMetricsRecorder> metrics_recorder_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      contextual_session_handle_;
  std::unique_ptr<TestEmbedder> embedder_;
  std::unique_ptr<ComposeboxHandler> handler_;
};

TEST_F(ComposeboxHandlerTest, SetDeepSearchMode) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  query_controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting(
          [&](ComposeboxQueryController::QueryControllerState state) {
            if (state == ComposeboxQueryController::QueryControllerState::
                             kClusterInfoReceived) {
              run_loop.Quit();
            }
          }));

  // Start the session.
  EXPECT_CALL(query_controller(), InitializeIfNeeded)
      .Times(1)
      .WillOnce(testing::Invoke(&query_controller(),
                                &MockQueryController::InitializeIfNeededBase));
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
      "ContextualSearch.Tools.DeepSearch.NewTabPage",
      contextual_search::AimToolState::kEnabled, 1);
  SubmitQueryAndWaitForNavigation();
  GURL query_url_dr =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  EXPECT_TRUE(net::GetValueForKeyInQuery(query_url_dr, "dr", &dr_param));
  EXPECT_EQ("1", dr_param);

  // Submitting after disabling deep search.
  handler().SetDeepSearchMode(false);
  histogram_tester().ExpectTotalCount(
      "ContextualSearch.Tools.DeepSearch.NewTabPage", 2);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.DeepSearch.NewTabPage",
      contextual_search::AimToolState::kEnabled, 1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.DeepSearch.NewTabPage",
      contextual_search::AimToolState::kDisabled, 1);
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
      base::BindLambdaForTesting(
          [&](ComposeboxQueryController::QueryControllerState state) {
            if (state == ComposeboxQueryController::QueryControllerState::
                             kClusterInfoReceived) {
              run_loop.Quit();
            }
          }));

  // Start the session.
  EXPECT_CALL(query_controller(), InitializeIfNeeded)
      .Times(1)
      .WillOnce(testing::Invoke(&query_controller(),
                                &MockQueryController::InitializeIfNeededBase));
  handler().NotifySessionStarted();
  run_loop.Run();

  // Submitting with create image mode enabled.
  handler().SetCreateImageMode(true, /*image_present= */ false);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Tools.CreateImages.NewTabPage",
      contextual_search::AimToolState::kEnabled, 1);
  SubmitQueryAndWaitForNavigation();
  GURL query_url_create_image =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  std::string imgn_param;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(query_url_create_image, "imgn", &imgn_param));
  EXPECT_EQ("1", imgn_param);

  // Submitting with create image mode disabled.
  handler().SetCreateImageMode(false, /*image_present= */ false);
  histogram_tester().ExpectTotalCount(
      "ContextualSearch.Tools.CreateImages.NewTabPage", 2);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.CreateImages.NewTabPage",
      contextual_search::AimToolState::kEnabled, 1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.CreateImages.NewTabPage",
      contextual_search::AimToolState::kDisabled, 1);
  SubmitQueryAndWaitForNavigation();
  GURL query_url_disabled_create_image =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  EXPECT_FALSE(net::GetValueForKeyInQuery(query_url_disabled_create_image,
                                          "imgn", &imgn_param));
}

TEST_F(ComposeboxHandlerTest, DeleteFileAndSubmitQuery) {
  std::string file_type = ".Image";
  std::string file_status = ".NotUploaded";
  std::unique_ptr<contextual_search::FileInfo> file_info =
      std::make_unique<contextual_search::FileInfo>();
  file_info->file_name = "test.png";
  file_info->mime_type = lens::MimeType::kImage;
  file_info->upload_status = contextual_search::FileUploadStatus::kNotUploaded;
  file_info->tab_session_id = SessionID::FromSerializedValue(123);
  base::UnguessableToken delete_file_token = base::UnguessableToken::Create();
  base::UnguessableToken token_arg;
  EXPECT_CALL(query_controller(), GetFileInfo(delete_file_token))
      .Times(2)
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

TEST_F(ComposeboxHandlerTest, SubmitQueryWithToolMetric) {
  // Submit with no tools enabled.
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.SubmissionType.NewTabPage",
      contextual_search::SubmissionType::kDefault, 1);

  // Submitting with deep search mode enabled.
  handler().SetDeepSearchMode(true);
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.SubmissionType.NewTabPage",
      contextual_search::SubmissionType::kDeepSearch, 1);

  // Submitting with create image mode enabled.
  handler().SetCreateImageMode(true, /*image_present= */ false);
  SubmitQueryAndWaitForNavigation();
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Tools.SubmissionType.NewTabPage",
      contextual_search::SubmissionType::kCreateImages, 1);

  histogram_tester().ExpectTotalCount(
      "ContextualSearch.Tools.SubmissionType.NewTabPage", 3);
}

TEST_F(ComposeboxHandlerTest, ContextMenu_Shows) {
  handler().ShowContextMenu(gfx::Point());
  EXPECT_TRUE(embedder().context_menu_shown());
}
