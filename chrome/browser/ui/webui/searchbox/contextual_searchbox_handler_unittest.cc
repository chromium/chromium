// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/internal/test_composebox_query_controller.h"
#include "components/contextual_search/pref_names.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/contextual_search_mojom_traits.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/web_ui_util.h"

using contextual_search::SessionState;

namespace {
constexpr char kClientUploadDurationQueryParameter[] = "cud";
constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";
constexpr char kQueryText[] = "query";
constexpr char kUdmQueryParameter[] = "udm";
constexpr char kUdmQueryParameterValue[] = "50";

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

class FakeContextualSearchboxHandler : public ContextualSearchboxHandler {
 public:
  FakeContextualSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxController> controller,
      GetSessionHandleCallback get_session_callback)
      : ContextualSearchboxHandler(std::move(pending_page_handler),
                                   profile,
                                   web_contents,
                                   std::move(controller),
                                   std::move(get_session_callback)) {}
  ~FakeContextualSearchboxHandler() override = default;

  // searchbox::mojom::PageHandler
  void ExecuteAction(uint8_t line,
                     uint8_t action_index,
                     const GURL& url,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override {}
  void OnThumbnailRemoved() override {}

  contextual_search::ContextualSearchMetricsRecorder* GetMetricsRecorder() {
    return ContextualSearchboxHandler::GetMetricsRecorder();
  }

  void NotifySessionStateChanged(
      contextual_search::SessionState session_state) {
    GetContextualSessionHandle()
        ->GetMetricsRecorder()
        ->NotifySessionStateChanged(session_state);
  }

  contextual_search::InputStateModel* input_state_model() {
    return input_state_model_.get();
  }
};
}  // namespace

// TODO(crbug.com/458086158): Make dedicated unit tests for the
// ContextualSessionHandle for testing query controller and metrics recorder
// interactions.
class ContextualSearchboxHandlerTest
    : public ContextualSearchboxHandlerTestHarness {
 public:
  ~ContextualSearchboxHandlerTest() override = default;

  void SetUp() override {
    ContextualSearchboxHandlerTestHarness::SetUp();

    auto query_controller_config_params = std::make_unique<
        contextual_search::ContextualSearchContextController::ConfigParams>();
    query_controller_config_params->send_lns_surface = false;
    query_controller_config_params->enable_viewport_images = true;
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        version_info::Channel::UNKNOWN, "en-US", template_url_service(),
        fake_variations_client(), std::move(query_controller_config_params));
    query_controller_ = query_controller_ptr.get();

    auto metrics_recorder_ptr =
        std::make_unique<MockContextualSearchMetricsRecorder>();

    service_ = ContextualSearchServiceFactory::GetForProfile(profile());
    contextual_session_handle_ = service_->CreateSessionForTesting(
        std::move(query_controller_ptr), std::move(metrics_recorder_ptr));
    // Check the search content sharing settings to notify the session handle
    // that the policy has been checked.
    contextual_session_handle_->CheckSearchContentSharingSettings(
        profile()->GetPrefs());

    web_contents()->SetDelegate(&delegate_);
    handler_ = std::make_unique<FakeContextualSearchboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
        web_contents(),
        std::make_unique<OmniboxController>(
            std::make_unique<TestOmniboxClient>()),
        base::BindLambdaForTesting(
            [&]() { return contextual_session_handle_.get(); }));
    handler_->SetPage(mock_searchbox_page_.BindAndGetRemote());

    ON_CALL(query_controller(), CreateSearchUrl)
        .WillByDefault(
            [](auto&& request_info, base::OnceCallback<void(GURL)> callback) {
              GURL url("https://www.google.com/search?q=" +
                       request_info->query_text);
              for (auto const& [key, val] : request_info->additional_params) {
                url = net::AppendOrReplaceQueryParameter(url, key, val);
              }
              url = net::AppendOrReplaceQueryParameter(
                  url, kQuerySubmissionTimeQueryParameter, "0");
              url = net::AppendOrReplaceQueryParameter(
                  url, kClientUploadDurationQueryParameter, "0");
              std::move(callback).Run(url);
            });
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

  FakeContextualSearchboxHandler& handler() { return *handler_; }
  MockQueryController& query_controller() { return *query_controller_; }

  MockContextualSearchMetricsRecorder* GetMetricsRecorderPtr() {
    if (handler_) {
      /* Cast since what we pass into the handler was a mock version to begin
       * with. */
      return static_cast<MockContextualSearchMetricsRecorder*>(
          handler_->GetMetricsRecorder());
    }
    return nullptr;
  }

  void TearDown() override {
    query_controller_ = nullptr;
    metrics_recorder_ = nullptr;
    handler_.reset();
    service_ = nullptr;
    ContextualSearchboxHandlerTestHarness::TearDown();
  }

 protected:
  testing::NiceMock<MockSearchboxPage> mock_searchbox_page_;
  std::unique_ptr<FakeContextualSearchboxHandler> handler_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      contextual_session_handle_;

 private:
  TestWebContentsDelegate delegate_;
  raw_ptr<MockQueryController> query_controller_;
  raw_ptr<contextual_search::ContextualSearchService> service_;
  raw_ptr<MockContextualSearchMetricsRecorder> metrics_recorder_;
};

TEST_F(ContextualSearchboxHandlerTest, SessionStarted) {
  SessionState state_arg = SessionState::kNone;
  auto* metrics_recorder_ptr = GetMetricsRecorderPtr();
  ASSERT_THAT(metrics_recorder_ptr, testing::NotNull());

  EXPECT_CALL(query_controller(), InitializeIfNeeded);
  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged(testing::_))
      .WillOnce(testing::SaveArg<0>(&state_arg));
  handler().NotifySessionStarted();
  EXPECT_EQ(state_arg, SessionState::kSessionStarted);
}

TEST_F(ContextualSearchboxHandlerTest, ActivateMetricsFunnel) {
  auto* metrics_recorder_ptr = GetMetricsRecorderPtr();
  ASSERT_THAT(metrics_recorder_ptr, testing::NotNull());

  EXPECT_CALL(*metrics_recorder_ptr, ActivateMetricsFunnel("AiMode")).Times(1);
  handler().ActivateMetricsFunnel("AiMode");
}

TEST_F(ContextualSearchboxHandlerTest, AddFile_Pdf) {
  searchbox::mojom::SelectedFileInfoPtr file_info =
      searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.pdf";
  file_info->selection_time = base::Time::Now();
  file_info->mime_type = "application/pdf";

  std::vector<uint8_t> test_data = {1, 2, 3, 4};
  auto test_data_span = base::span<const uint8_t>(test_data);
  mojo_base::BigBuffer file_data(test_data_span);

  base::MockCallback<ComposeboxHandler::AddFileContextCallback> callback;
  base::UnguessableToken controller_file_info_token;
  std::optional<base::UnguessableToken> callback_token;
  EXPECT_CALL(query_controller(), StartFileUploadFlow)
      .WillOnce(testing::SaveArg<0>(&controller_file_info_token));
  EXPECT_CALL(callback, Run).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    callback_token = result.value();
  });
  handler().AddFileContext(std::move(file_info), std::move(file_data),
                           callback.Get());

  EXPECT_EQ(callback_token, controller_file_info_token);
}

TEST_F(ContextualSearchboxHandlerTest, AddFile_Image) {
  searchbox::mojom::SelectedFileInfoPtr file_info =
      searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.png";
  file_info->selection_time = base::Time::Now();
  file_info->mime_type = "application/image";

  std::vector<uint8_t> test_data = {1, 2, 3, 4};
  auto test_data_span = base::span<const uint8_t>(test_data);
  mojo_base::BigBuffer file_data(test_data_span);

  std::optional<lens::ImageEncodingOptions> image_options;
  base::UnguessableToken controller_file_info_token;
  EXPECT_CALL(query_controller(), StartFileUploadFlow)
      .WillOnce([&](const base::UnguessableToken& file_token, auto,
                    std::optional<lens::ImageEncodingOptions> options_arg) {
        controller_file_info_token = file_token;
        image_options = std::move(options_arg);
      });
  base::MockCallback<ComposeboxHandler::AddFileContextCallback> callback;
  std::optional<base::UnguessableToken> callback_token;
  EXPECT_CALL(callback, Run).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    callback_token = result.value();
  });

  handler().AddFileContext(std::move(file_info), std::move(file_data),
                           callback.Get());

  EXPECT_EQ(callback_token, controller_file_info_token);
  EXPECT_TRUE(image_options.has_value());

  auto image_upload = scoped_config().Get().config.composebox().image_upload();
  EXPECT_EQ(image_options->max_height,
            image_upload.downscale_max_image_height());
  EXPECT_EQ(image_options->max_size, image_upload.downscale_max_image_size());
  EXPECT_EQ(image_options->max_width, image_upload.downscale_max_image_width());
  EXPECT_EQ(image_options->compression_quality,
            image_upload.image_compression_quality());
}

TEST_F(ContextualSearchboxHandlerTest, ClearFiles) {
  searchbox::mojom::SelectedFileInfoPtr file_info =
      searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.png";
  file_info->selection_time = base::Time::Now();
  file_info->mime_type = "application/image";

  std::vector<uint8_t> test_data = {1, 2, 3, 4};
  auto test_data_span = base::span<const uint8_t>(test_data);
  mojo_base::BigBuffer file_data(test_data_span);

  std::optional<lens::ImageEncodingOptions> image_options;
  base::UnguessableToken controller_file_info_token;
  EXPECT_CALL(query_controller(), StartFileUploadFlow)
      .WillOnce([&](const base::UnguessableToken& file_token, auto,
                    std::optional<lens::ImageEncodingOptions> options_arg) {
        controller_file_info_token = file_token;
        image_options = std::move(options_arg);
      });
  base::MockCallback<ComposeboxHandler::AddFileContextCallback> callback;
  std::optional<base::UnguessableToken> callback_token;
  EXPECT_CALL(callback, Run).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    callback_token = result.value();
  });

  handler().AddFileContext(std::move(file_info), std::move(file_data),
                           callback.Get());
  EXPECT_EQ(handler().GetUploadedContextTokens().size(), 1u);

  EXPECT_CALL(query_controller(), ClearFiles).Times(0);
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(1);
  handler().ClearFiles(/*should_block_auto_suggested_tabs=*/false);
  EXPECT_EQ(handler().GetUploadedContextTokens().size(), 0u);
}

TEST_F(ContextualSearchboxHandlerTest, AddFile_PolicyDisabled) {
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  searchbox::mojom::SelectedFileInfoPtr file_info =
      searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.pdf";
  file_info->selection_time = base::Time::Now();
  file_info->mime_type = "application/pdf";

  std::vector<uint8_t> test_data = {1, 2, 3, 4};
  auto test_data_span = base::span<const uint8_t>(test_data);
  mojo_base::BigBuffer file_data(test_data_span);

  base::MockCallback<ComposeboxHandler::AddFileContextCallback> callback;
  base::expected<base::UnguessableToken,
                 contextual_search::FileUploadErrorType>
      callback_result =
          base::ok(base::UnguessableToken());  // Initialize with dummy

  EXPECT_CALL(query_controller(), StartFileUploadFlow).Times(0);
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&callback_result));
  handler().AddFileContext(std::move(file_info), std::move(file_data),
                           callback.Get());

  EXPECT_FALSE(callback_result.has_value());
  EXPECT_EQ(callback_result.error(),
            contextual_search::FileUploadErrorType::kBrowserProcessingError);
}

TEST_F(ContextualSearchboxHandlerTest, AddFile_PolicyToggled) {
  // Start with disabled.
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  searchbox::mojom::SelectedFileInfoPtr file_info_1 =
      searchbox::mojom::SelectedFileInfo::New();
  file_info_1->file_name = "test1.pdf";
  file_info_1->mime_type = "application/pdf";
  std::vector<uint8_t> test_data_1 = {1};
  auto test_data_span_1 = base::span<const uint8_t>(test_data_1);
  mojo_base::BigBuffer file_data_1(test_data_span_1);

  base::MockCallback<ComposeboxHandler::AddFileContextCallback> callback_1;
  base::expected<base::UnguessableToken,
                 contextual_search::FileUploadErrorType>
      callback_result_1 =
          base::ok(base::UnguessableToken());  // Initialize with dummy

  EXPECT_CALL(query_controller(), StartFileUploadFlow).Times(0);
  EXPECT_CALL(callback_1, Run)
      .WillOnce(testing::SaveArg<0>(&callback_result_1));
  handler().AddFileContext(std::move(file_info_1), std::move(file_data_1),
                           callback_1.Get());

  EXPECT_FALSE(callback_result_1.has_value());
  EXPECT_EQ(callback_result_1.error(),
            contextual_search::FileUploadErrorType::kBrowserProcessingError);

  // Enable policy.
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  searchbox::mojom::SelectedFileInfoPtr file_info_2 =
      searchbox::mojom::SelectedFileInfo::New();
  file_info_2->file_name = "test2.pdf";
  file_info_2->mime_type = "application/pdf";
  std::vector<uint8_t> test_data_2 = {2};
  auto test_data_span_2 = base::span<const uint8_t>(test_data_2);
  mojo_base::BigBuffer file_data_2(test_data_span_2);

  base::MockCallback<ComposeboxHandler::AddFileContextCallback> callback_2;
  std::optional<base::UnguessableToken> callback_token_2;

  EXPECT_CALL(query_controller(), StartFileUploadFlow).Times(1);
  EXPECT_CALL(callback_2, Run).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    callback_token_2 = result.value();
  });
  handler().AddFileContext(std::move(file_info_2), std::move(file_data_2),
                           callback_2.Get());

  EXPECT_TRUE(callback_token_2.has_value());
}

TEST_F(ContextualSearchboxHandlerTest, AddFileFromBrowser_PolicyDisabled) {
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  std::string file_name = "test.pdf";
  std::string mime_type = "application/pdf";
  std::vector<uint8_t> test_data = {1, 2, 3, 4};
  auto test_data_span = base::span<const uint8_t>(test_data);
  mojo_base::BigBuffer file_data(test_data_span);
  std::optional<lens::ImageEncodingOptions> image_options;

  base::MockCallback<ComposeboxHandler::AddFileContextCallback> callback;
  base::expected<base::UnguessableToken,
                 contextual_search::FileUploadErrorType>
      callback_result =
          base::ok(base::UnguessableToken());  // Initialize with dummy

  EXPECT_CALL(query_controller(), StartFileUploadFlow).Times(0);
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&callback_result));
  handler().AddFileContextFromBrowser(file_name, mime_type,
                                      std::move(file_data), image_options,
                                      callback.Get());

  EXPECT_FALSE(callback_result.has_value());
  EXPECT_EQ(callback_result.error(),
            contextual_search::FileUploadErrorType::kBrowserProcessingError);
}

TEST_F(ContextualSearchboxHandlerTest, AddFileFromBrowser_PolicyEnabled) {
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  std::string file_name = "test.pdf";
  std::string mime_type = "application/pdf";
  std::vector<uint8_t> test_data = {1, 2, 3, 4};
  auto test_data_span = base::span<const uint8_t>(test_data);
  mojo_base::BigBuffer file_data(test_data_span);
  std::optional<lens::ImageEncodingOptions> image_options;

  base::MockCallback<ComposeboxHandler::AddFileContextCallback> callback;
  base::UnguessableToken controller_file_info_token;
  std::optional<base::UnguessableToken> callback_token;

  EXPECT_CALL(query_controller(), StartFileUploadFlow)
      .WillOnce(testing::SaveArg<0>(&controller_file_info_token));
  EXPECT_CALL(callback, Run).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    callback_token = result.value();
  });
  handler().AddFileContextFromBrowser(file_name, mime_type,
                                      std::move(file_data), image_options,
                                      callback.Get());

  EXPECT_EQ(callback_token, controller_file_info_token);
  EXPECT_TRUE(callback_token.has_value());
}

TEST_F(ContextualSearchboxHandlerTest, SubmitQuery) {
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

  std::vector<SessionState> session_states;
  auto* metrics_recorder_ptr = GetMetricsRecorderPtr();
  ASSERT_THAT(metrics_recorder_ptr, testing::NotNull());

  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged(testing::_))
      .Times(3)
      .WillRepeatedly([&](SessionState session_state) {
        session_states.push_back(session_state);
      });
  EXPECT_CALL(*metrics_recorder_ptr,
              NotifyQuerySubmitted(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          metrics_recorder_ptr,
          &MockContextualSearchMetricsRecorder::NotifyQuerySubmittedBase));

  // Start the session.
  EXPECT_CALL(query_controller(), InitializeIfNeeded)
      .Times(1)
      .WillOnce(testing::Invoke(&query_controller(),
                                &MockQueryController::InitializeIfNeededBase));

  // There should be no delayed uploads if there are no cached tab snapshot.
  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(0);

  handler().NotifySessionStarted();
  run_loop.Run();

  SubmitQueryAndWaitForNavigation();

  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
      search_url_request_info = std::make_unique<
          ComposeboxQueryController::CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = kQueryText;
  search_url_request_info->query_start_time = base::Time::Now();
  search_url_request_info->additional_params = {
      {kUdmQueryParameter, kUdmQueryParameterValue}};
  base::test::TestFuture<GURL> future;
  query_controller().CreateSearchUrl(std::move(search_url_request_info),
                                     future.GetCallback());
  GURL expected_url = future.Take();
  GURL actual_url =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();

  // Ensure navigation occurred.
  EXPECT_EQ(StripTimestampsFromAimUrl(expected_url),
            StripTimestampsFromAimUrl(actual_url));

  EXPECT_THAT(session_states,
              testing::ElementsAre(SessionState::kSessionStarted,
                                   SessionState::kQuerySubmitted,
                                   SessionState::kNavigationOccurred));
}

TEST_F(ContextualSearchboxHandlerTest, SubmitQuery_DelayUpload) {
  // Arrange
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

  std::vector<SessionState> session_states;
  auto* metrics_recorder_ptr = GetMetricsRecorderPtr();
  ASSERT_THAT(metrics_recorder_ptr, testing::NotNull());

  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged(testing::_))
      .Times(3)
      .WillRepeatedly([&](SessionState session_state) {
        session_states.push_back(session_state);
      });
  EXPECT_CALL(*metrics_recorder_ptr,
              NotifyQuerySubmitted(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          metrics_recorder_ptr,
          &MockContextualSearchMetricsRecorder::NotifyQuerySubmittedBase));

  // Start the session.
  EXPECT_CALL(query_controller(), InitializeIfNeeded)
      .Times(1)
      .WillOnce(testing::Invoke(&query_controller(),
                                &MockQueryController::InitializeIfNeededBase));

  // Set a cached tab context snapshot.
  auto token = base::UnguessableToken::Create();
  handler()
      .GetContextualSessionHandle()
      ->GetUploadedContextTokensForTesting()
      .push_back(token);
  handler().tab_context_snapshot_ =
      std::make_pair(token, std::make_unique<lens::ContextualInputData>());
  ASSERT_TRUE(handler().tab_context_snapshot_.has_value());

  // The file should be uploaded if there is a cached tab snapshot.
  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(1);

  handler().NotifySessionStarted();
  run_loop.Run();

  // Act
  SubmitQueryAndWaitForNavigation();

  // Assert
  // Once the cached tab snapshot is uploaded, it should be cleared.
  ASSERT_FALSE(handler().tab_context_snapshot_.has_value());

  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
      search_url_request_info = std::make_unique<
          ComposeboxQueryController::CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = kQueryText;
  search_url_request_info->query_start_time = base::Time::Now();
  search_url_request_info->file_tokens.push_back(token);
  search_url_request_info->additional_params = {
      {kUdmQueryParameter, kUdmQueryParameterValue}};
  base::test::TestFuture<GURL> future;
  query_controller().CreateSearchUrl(std::move(search_url_request_info),
                                     future.GetCallback());
  GURL expected_url = future.Take();
  GURL actual_url =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();

  // Ensure navigation occurred.
  EXPECT_EQ(StripTimestampsFromAimUrl(expected_url),
            StripTimestampsFromAimUrl(actual_url));

  EXPECT_THAT(session_states,
              testing::ElementsAre(SessionState::kSessionStarted,
                                   SessionState::kQuerySubmitted,
                                   SessionState::kNavigationOccurred));
}

TEST_F(ContextualSearchboxHandlerTest, OnInputStateChanged) {
  omnibox::InputState received_state_1;
  omnibox::InputState received_state_2;

  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged)
      .Times(2)
      .WillOnce(
          [&](const omnibox::InputState& state) { received_state_1 = state; })
      .WillOnce(
          [&](const omnibox::InputState& state) { received_state_2 = state; });
  EXPECT_CALL(*GetMetricsRecorderPtr(),
              RecordToolMode(composebox_query::mojom::ToolMode::kCanvas))
      .WillOnce(testing::Invoke(
          GetMetricsRecorderPtr(),
          &MockContextualSearchMetricsRecorder::RecordToolModeBase));

  handler().SetActiveToolMode(omnibox::ToolMode::TOOL_MODE_CANVAS);
  mock_searchbox_page_.FlushForTesting();
  EXPECT_EQ(received_state_1.active_tool, omnibox::ToolMode::TOOL_MODE_CANVAS);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Tools.NewTabPage",
      composebox_query::mojom::ToolMode::kCanvas, 1);

  EXPECT_CALL(
      *GetMetricsRecorderPtr(),
      RecordModelMode(composebox_query::mojom::ModelMode::kGeminiRegular))
      .WillOnce(testing::Invoke(
          GetMetricsRecorderPtr(),
          &MockContextualSearchMetricsRecorder::RecordModelModeBase));

  handler().SetActiveModelMode(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  mock_searchbox_page_.FlushForTesting();
  EXPECT_EQ(received_state_2.active_tool, omnibox::ToolMode::TOOL_MODE_CANVAS);
  EXPECT_EQ(received_state_2.active_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Models.NewTabPage",
      composebox_query::mojom::ModelMode::kGeminiRegular, 1);
}
TEST_F(ContextualSearchboxHandlerTest, SubmitQueryWithAdditionalParams) {
  // Ensure udm param is always set as an additional param.
  SubmitQueryAndWaitForNavigation();
  GURL query_url =
      web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  std::string udm_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(query_url, "udm", &udm_param));
  EXPECT_EQ("50", udm_param);
}

TEST_F(ContextualSearchboxHandlerTest, QueryAutocomplete_SetsLensInputs) {
  // Set suggest inputs on the client.
  lens::proto::LensOverlaySuggestInputs suggest_inputs;
  suggest_inputs.set_encoded_image_signals("xyz");
  EXPECT_CALL(*static_cast<TestOmniboxClient*>(
                  handler().omnibox_controller()->client()),
              GetLensOverlaySuggestInputs())
      .WillRepeatedly(testing::Return(suggest_inputs));

  // Set input state where `image_gen_upload_active` is false and active_tool is
  // not CANVAS.
  omnibox::InputState state;
  state.image_gen_upload_active = false;
  state.active_tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  handler().input_state_model()->set_state_for_testing(state);
  handler().OnInputStateChangedForTesting(state);

  // Set mock AutocompleteController.
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  AutocompleteInput input;
  EXPECT_CALL(*autocomplete_controller, Start(_))
      .WillOnce(testing::SaveArg<0>(&input));
  handler().omnibox_controller()->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));

  handler().QueryAutocomplete(u"test", false);

  EXPECT_TRUE(input.lens_overlay_suggest_inputs().has_value());
  EXPECT_EQ(input.lens_overlay_suggest_inputs()->encoded_image_signals(),
            "xyz");
}

TEST_F(ContextualSearchboxHandlerTest,
       QueryAutocomplete_SkipsLensInputs_InToolModes) {
  lens::proto::LensOverlaySuggestInputs suggest_inputs;
  suggest_inputs.set_encoded_image_signals("xyz");
  EXPECT_CALL(*static_cast<TestOmniboxClient*>(
                  handler().omnibox_controller()->client()),
              GetLensOverlaySuggestInputs())
      .WillRepeatedly(testing::Return(suggest_inputs));

  // 1. Case: `image_gen_upload_active = true`.
  {
    omnibox::InputState state;
    state.image_gen_upload_active = true;
    state.active_tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
    handler().input_state_model()->set_state_for_testing(state);
    handler().OnInputStateChangedForTesting(state);

    auto autocomplete_controller =
        std::make_unique<testing::NiceMock<MockAutocompleteController>>(
            std::make_unique<MockAutocompleteProviderClient>(), 0);
    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller, Start(_))
        .WillOnce(testing::SaveArg<0>(&input));
    handler().omnibox_controller()->SetAutocompleteControllerForTesting(
        std::move(autocomplete_controller));

    handler().QueryAutocomplete(u"test", false);
    EXPECT_FALSE(input.lens_overlay_suggest_inputs().has_value());
  }

  // 2. Case: `active_tool = TOOL_MODE_CANVAS`.
  {
    omnibox::InputState state;
    state.image_gen_upload_active = false;
    state.active_tool = omnibox::ToolMode::TOOL_MODE_CANVAS;
    handler().input_state_model()->set_state_for_testing(state);
    handler().OnInputStateChangedForTesting(state);

    auto autocomplete_controller =
        std::make_unique<testing::NiceMock<MockAutocompleteController>>(
            std::make_unique<MockAutocompleteProviderClient>(), 0);
    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller, Start(_))
        .WillOnce(testing::SaveArg<0>(&input));
    handler().omnibox_controller()->SetAutocompleteControllerForTesting(
        std::move(autocomplete_controller));

    handler().QueryAutocomplete(u"test", false);
    EXPECT_FALSE(input.lens_overlay_suggest_inputs().has_value());
  }
}

class ContextualSearchboxHandlerTestTabsTest
    : public ContextualSearchboxHandlerTest {
 public:
  ContextualSearchboxHandlerTestTabsTest() = default;

  ~ContextualSearchboxHandlerTestTabsTest() override {
    // Break loop so we can deconstruct without dangling pointers.
    delegate_.SetBrowserWindowInterface(nullptr);
  }

  void SetUp() override {
    ContextualSearchboxHandlerTest::SetUp();
    tab_strip_model_ = std::make_unique<TabStripModel>(&delegate_, profile());
    ON_CALL(browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    delegate_.SetBrowserWindowInterface(&browser_window_interface_);
    webui::SetBrowserWindowInterface(web_contents(),
                                     &browser_window_interface_);
  }

  void TearDown() override {
    // Clear TabContextualizationController to avoid dangling pointers.
    if (tab_strip_model_) {
      for (int i = 0; i < tab_strip_model_->count(); ++i) {
        tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(i);
        if (tab && tab->GetTabFeatures()) {
          tab->GetTabFeatures()->SetTabContextualizationControllerForTesting(
              nullptr);
        }
      }
    }
    handler_.reset();
    tab_strip_model_.reset();
    ContextualSearchboxHandlerTest::TearDown();
  }

  base::TimeTicks IncrementTimeTicksAndGet() {
    last_active_time_ticks_ += base::Seconds(1);
    return last_active_time_ticks_;
  }

  TestTabStripModelDelegate* delegate() { return &delegate_; }
  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return &browser_window_interface_;
  }
  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile(),
                                                             nullptr);
  }

  tabs::TabInterface* AddTab(GURL url) {
    std::unique_ptr<content::WebContents> contents_unique_ptr =
        CreateWebContents();
    content::WebContentsTester::For(contents_unique_ptr.get())
        ->NavigateAndCommit(url);
    content::WebContents* content_ptr = contents_unique_ptr.get();
    content::WebContentsTester::For(content_ptr)
        ->SetLastActiveTimeTicks(IncrementTimeTicksAndGet());
    tab_strip_model()->AppendWebContents(std::move(contents_unique_ptr), true);
    tabs::TabInterface* tab_interface =
        tab_strip_model()->GetTabForWebContents(content_ptr);
    tabs::TabFeatures* const tab_features = tab_interface->GetTabFeatures();
    std::unique_ptr<TabUIHelper> tab_ui_helper =
        tabs::TabFeatures::GetUserDataFactoryForTesting()
            .CreateInstance<TabUIHelper>(*tab_interface, *tab_interface);
    tab_features->SetTabUIHelperForTesting(std::move(tab_ui_helper));
    std::unique_ptr<lens::TabContextualizationController>
        tab_contextualization_controller =
            tabs::TabFeatures::GetUserDataFactoryForTesting()
                .CreateInstance<MockTabContextualizationController>(
                    *tab_interface, tab_interface);
    tab_features->SetTabContextualizationControllerForTesting(
        std::move(tab_contextualization_controller));
    std::unique_ptr<tabs::TabAlertController> tab_alert_controller =
        tabs::TabFeatures::GetUserDataFactoryForTesting()
            .CreateInstance<tabs::TabAlertController>(*tab_interface,
                                                      *tab_interface);
    tab_features->SetTabAlertControllerForTesting(
        std::move(tab_alert_controller));
    return tab_interface;
  }

 private:
  base::TimeTicks last_active_time_ticks_;
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  ui::UnownedUserDataHost user_data_host_;
  MockBrowserWindowInterface browser_window_interface_;
  base::HistogramTester histogram_tester_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(ContextualSearchboxHandlerTestTabsTest, AddTabContext) {
  auto sample_url = GURL("https://www.google.com");
  tabs::TabInterface* tab = AddTab(sample_url);
  const int sample_tab_id = tab->GetHandle().raw_value();

  tabs::TabFeatures* tab_features = tab->GetTabFeatures();
  MockTabContextualizationController* tab_contextualization_controller =
      static_cast<MockTabContextualizationController*>(
          tab_features->tab_contextualization_controller());
  EXPECT_CALL(*tab_contextualization_controller, GetPageContext(testing::_))
      .Times(1)
      .WillRepeatedly(
          [](lens::TabContextualizationController::GetPageContextCallback
                 callback) {
            std::move(callback).Run(
                std::make_unique<lens::ContextualInputData>());
          });

  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(1);
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(1);
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  auto sample_contextual_input_data =
      std::make_unique<lens::ContextualInputData>();
  sample_contextual_input_data->page_url = sample_url;
  handler().AddTabContext(sample_tab_id, /*delay_upload=*/false,
                          callback.Get());

  // Flush the mojo pipe to ensure the callback is run.
  mock_searchbox_page_.FlushForTesting();
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, AddTabContextNotFound) {
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback;
  base::expected<base::UnguessableToken,
                 contextual_search::FileUploadErrorType>
      callback_result =
          base::ok(base::UnguessableToken());  // Initialize with dummy

  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&callback_result));

  handler().AddTabContext(0, false, callback.Get());

  // Flush the mojo pipe to ensure the callback is run.
  mock_searchbox_page_.FlushForTesting();

  EXPECT_FALSE(callback_result.has_value());
  EXPECT_EQ(callback_result.error(),
            contextual_search::FileUploadErrorType::kBrowserProcessingError);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, AddTabContext_PolicyDisabled) {
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  auto sample_url = GURL("https://www.google.com");
  tabs::TabInterface* tab = AddTab(sample_url);
  const int sample_tab_id = tab->GetHandle().raw_value();

  EXPECT_CALL(query_controller(), StartFileUploadFlow).Times(0);
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback;
  base::expected<base::UnguessableToken,
                 contextual_search::FileUploadErrorType>
      callback_result =
          base::ok(base::UnguessableToken());  // Initialize with dummy

  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&callback_result));

  handler().AddTabContext(sample_tab_id, /*delay_upload=*/false,
                          callback.Get());

  // Flush the mojo pipe to ensure the callback is run.
  mock_searchbox_page_.FlushForTesting();

  EXPECT_FALSE(callback_result.has_value());
  EXPECT_EQ(callback_result.error(),
            contextual_search::FileUploadErrorType::kBrowserProcessingError);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, AddTabContext_DelayUpload) {
  // Arrange
  auto sample_url = GURL("https://www.google.com");
  tabs::TabInterface* tab = AddTab(sample_url);
  const int sample_tab_id = tab->GetHandle().raw_value();

  contextual_search::FileUploadStatus status;

  tabs::TabFeatures* tab_features = tab->GetTabFeatures();
  MockTabContextualizationController* tab_contextualization_controller =
      static_cast<MockTabContextualizationController*>(
          tab_features->tab_contextualization_controller());
  EXPECT_CALL(*tab_contextualization_controller, GetPageContext(testing::_))
      .Times(1)
      .WillRepeatedly(
          [](lens::TabContextualizationController::GetPageContextCallback
                 callback) {
            std::move(callback).Run(
                std::make_unique<lens::ContextualInputData>());
          });

  // `ComposeboxQueryController::StartFileUploadFlow` should not be called when
  // tab context is added, upload should be delayed, and the tab context
  // should be cached.
  ASSERT_FALSE(handler().tab_context_snapshot_.has_value());
  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(0);
  EXPECT_CALL(mock_searchbox_page_, OnContextualInputStatusChanged)
      .Times(1)
      .WillOnce(
          [&status](const base::UnguessableToken& file_token,
                    contextual_search::FileUploadStatus file_upload_status,
                    std::optional<contextual_search::FileUploadErrorType>
                        file_upload_error_type) {
            status = file_upload_status;
          });
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(1);

  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  auto sample_contextual_input_data =
      std::make_unique<lens::ContextualInputData>();
  sample_contextual_input_data->page_url = sample_url;

  // Act
  handler().AddTabContext(sample_tab_id, /*delay_upload=*/true, callback.Get());
  // Flush the mojo pipe to ensure the callback is run and captures the status.
  mock_searchbox_page_.FlushForTesting();

  // Assert
  ASSERT_TRUE(handler().tab_context_snapshot_.has_value());
  ASSERT_TRUE(handler().context_input_data().has_value());
  ASSERT_EQ(contextual_search::FileUploadStatus::kProcessing, status);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, DeleteContext_DelayUpload) {
  // Arrange
  auto sample_url = GURL("https://www.google.com");
  tabs::TabInterface* tab = AddTab(sample_url);
  const int sample_tab_id = tab->GetHandle().raw_value();

  tabs::TabFeatures* tab_features = tab->GetTabFeatures();
  MockTabContextualizationController* tab_contextualization_controller =
      static_cast<MockTabContextualizationController*>(
          tab_features->tab_contextualization_controller());
  EXPECT_CALL(*tab_contextualization_controller, GetPageContext(testing::_))
      .Times(1)
      .WillRepeatedly(
          [](lens::TabContextualizationController::GetPageContextCallback
                 callback) {
            std::move(callback).Run(
                std::make_unique<lens::ContextualInputData>());
          });

  // `ComposeboxQueryController::StartFileUploadFlow` should not be called when
  // tab context is added and upload should be delayed.
  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(0);
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(2);
  base::test::TestFuture<base::expected<base::UnguessableToken,
                                        contextual_search::FileUploadErrorType>>
      future;
  auto sample_contextual_input_data =
      std::make_unique<lens::ContextualInputData>();
  sample_contextual_input_data->page_url = sample_url;
  handler().AddTabContext(sample_tab_id, /*delay_upload=*/true,
                          future.GetCallback());
  // Flush the mojo pipe to ensure the callback is run.
  mock_searchbox_page_.FlushForTesting();

  ASSERT_TRUE(future.Get().has_value());
  auto file_token = future.Get().value();
  ASSERT_TRUE(handler().tab_context_snapshot_.has_value());
  ASSERT_EQ(file_token, handler().tab_context_snapshot_.value().first);

  // Act
  // Delete context.
  handler().DeleteContext(file_token, /*from_automatic_chip=*/false);
  mock_searchbox_page_.FlushForTesting();

  // Assert
  ASSERT_FALSE(handler().tab_context_snapshot_.has_value());
  ASSERT_FALSE(handler().context_input_data().has_value());
}

// Tests that context input data is set when there is delayed
// context, and cleared when in multi-context mode.
TEST_F(ContextualSearchboxHandlerTestTabsTest,
       ContextInputDataChangesOnMultiContext) {
  // Add delayed context and ensure context input data is set.
  auto url1 = GURL("https://www.google.com");
  tabs::TabInterface* tab1 = AddTab(url1);
  const int tab_id1 = tab1->GetHandle().raw_value();

  MockTabContextualizationController* tab_contextualization_controller1 =
      static_cast<MockTabContextualizationController*>(
          tab1->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*tab_contextualization_controller1, GetPageContext(testing::_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(3);

  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback;
  handler().AddTabContext(tab_id1, /*delay_upload=*/true, callback.Get());
  // Flush the mojo pipe to ensure the callback is run.
  mock_searchbox_page_.FlushForTesting();
  ASSERT_TRUE(handler().context_input_data().has_value());

  // Add non-delayed context and ensure context input data has no value.
  auto url2 = GURL("https://www.wikipedia.com");
  tabs::TabInterface* tab2 = AddTab(url2);
  const int tab_id2 = tab2->GetHandle().raw_value();
  MockTabContextualizationController* tab_contextualization_controller2 =
      static_cast<MockTabContextualizationController*>(
          tab2->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*tab_contextualization_controller2, GetPageContext(testing::_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(1);
  base::test::TestFuture<base::expected<base::UnguessableToken,
                                        contextual_search::FileUploadErrorType>>
      future;
  handler().AddTabContext(tab_id2, /*delay_upload=*/false,
                          future.GetCallback());
  mock_searchbox_page_.FlushForTesting();
  ASSERT_FALSE(handler().context_input_data().has_value());

  // Delete the non-delayed context and ensure context input data now has a
  // value again because the only context left is of delayed type.
  auto token = future.Get().value();
  handler().DeleteContext(token, /*from_automatic_chip=*/false);
  mock_searchbox_page_.FlushForTesting();
  mock_searchbox_page_.FlushForTesting();
  ASSERT_TRUE(handler().context_input_data().has_value());
}



TEST_F(ContextualSearchboxHandlerTestTabsTest, TabContextAddedMetric) {
  // Add a tab.
  tabs::TabInterface* tab = AddTab(GURL("https://example.com"));
  const int tab_id = tab->GetHandle().raw_value();

  // Mock the call to AddTabContext.
  MockTabContextualizationController* controller =
      static_cast<MockTabContextualizationController*>(
          tab->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*controller, GetPageContext(testing::_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });
  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(1);
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(1);

  auto* metrics_recorder_ptr = GetMetricsRecorderPtr();
  ASSERT_THAT(metrics_recorder_ptr, testing::NotNull());
  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged(testing::_))
      .WillRepeatedly(testing::Invoke(
          metrics_recorder_ptr,
          &MockContextualSearchMetricsRecorder::NotifySessionStateChangedBase));

  base::test::TestFuture<base::expected<base::UnguessableToken,
                                        contextual_search::FileUploadErrorType>>
      future;
  handler().NotifySessionStarted();
  handler().AddTabContext(tab_id, false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().has_value());

  // Check that the histogram was recorded.
  handler().NotifySessionStateChanged(SessionState::kSessionAbandoned);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabContextAdded.V2.NewTabPage", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabWithDuplicateTitleClicked.V2.NewTabPage", 0, 1);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest,
       TabStripModelObserverIsAddedWithValidSession) {
  EXPECT_CALL(mock_searchbox_page_, OnTabStripChanged).Times(1);
  handler().OnTabStripModelChanged(
      tab_strip_model(), TabStripModelChange(TabStripModelChange::Remove()),
      {});
  mock_searchbox_page_.FlushForTesting();
}

TEST_F(ContextualSearchboxHandlerTestTabsTest,
       TabStripModelObserverSelectivelyNotifies) {
  {
    // Tab insert updates notify.
    EXPECT_CALL(mock_searchbox_page_, OnTabStripChanged).Times(1);
    handler().OnTabStripModelChanged(
        tab_strip_model(), TabStripModelChange(TabStripModelChange::Insert()),
        {});
    mock_searchbox_page_.FlushForTesting();
  }
  {
    // Tab move updates don't notify.
    EXPECT_CALL(mock_searchbox_page_, OnTabStripChanged).Times(0);
    handler().OnTabStripModelChanged(
        tab_strip_model(), TabStripModelChange(TabStripModelChange::Move()),
        {});
    mock_searchbox_page_.FlushForTesting();
  }
}

// TODO(b:466469292): Figure out how to null-ify the session handle so we can
//   test the handler behaves correctly in that case.
TEST_F(ContextualSearchboxHandlerTestTabsTest,
       DISABLED_TabStripModelObserverIsNotAddedWithNullSession) {
  // Create a handler with a null session handle.
  auto handler_with_null_session =
      std::make_unique<FakeContextualSearchboxHandler>(
          mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
          web_contents(),
          std::make_unique<OmniboxController>(
              std::make_unique<TestOmniboxClient>()),
          base::BindLambdaForTesting(
              []() -> contextual_search::ContextualSearchSessionHandle* {
                return nullptr;
              }));

  // Use a new MockSearchboxPage for the new handler.
  testing::NiceMock<MockSearchboxPage> local_mock_searchbox_page;
  handler_with_null_session->SetPage(
      local_mock_searchbox_page.BindAndGetRemote());

  // The observer should not be added, so OnTabStripChanged should not be
  // called.
  EXPECT_CALL(local_mock_searchbox_page, OnTabStripChanged).Times(0);
  handler_with_null_session->OnTabStripModelChanged(tab_strip_model(), {}, {});
  local_mock_searchbox_page.FlushForTesting();
}

TEST_F(ContextualSearchboxHandlerTestTabsTest,
       TabWithDuplicateTitleClickedMetric) {
  // Add tabs with duplicate titles.
  tabs::TabInterface* tab_a1 = AddTab(GURL("https://a1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(0))
      ->SetTitle(u"Title A");
  tabs::TabInterface* tab_b1 = AddTab(GURL("https://b1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(1))
      ->SetTitle(u"Title B");
  AddTab(GURL("https://a2.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(2))
      ->SetTitle(u"Title A");

  // Mock tab upload flow.
  MockTabContextualizationController* controller_a1 =
      static_cast<MockTabContextualizationController*>(
          tab_a1->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*controller_a1, GetPageContext(testing::_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  MockTabContextualizationController* controller_b1 =
      static_cast<MockTabContextualizationController*>(
          tab_b1->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*controller_b1, GetPageContext(testing::_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });
  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(2);
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(2);

  auto* metrics_recorder_ptr = GetMetricsRecorderPtr();
  ASSERT_THAT(metrics_recorder_ptr, testing::NotNull());
  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged(testing::_))
      .WillRepeatedly(testing::Invoke(
          metrics_recorder_ptr,
          &MockContextualSearchMetricsRecorder::NotifySessionStateChangedBase));

  // Click on a tab with a duplicate title.
  base::test::TestFuture<base::expected<base::UnguessableToken,
                                        contextual_search::FileUploadErrorType>>
      future1;
  handler().NotifySessionStarted();
  handler().AddTabContext(tab_a1->GetHandle().raw_value(), false,
                          future1.GetCallback());
  ASSERT_TRUE(future1.Wait());
  EXPECT_TRUE(future1.Get().has_value());

  // Click on a tab with a unique title.
  base::test::TestFuture<base::expected<base::UnguessableToken,
                                        contextual_search::FileUploadErrorType>>
      future2;
  handler().AddTabContext(tab_b1->GetHandle().raw_value(), false,
                          future2.GetCallback());
  ASSERT_TRUE(future2.Wait());

  // End the session to log the metrics.
  handler().NotifySessionStateChanged(SessionState::kSessionAbandoned);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabWithDuplicateTitleClicked.V2.NewTabPage", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabContextAdded.V2.NewTabPage", 2, 1);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest,
       TabWithDuplicateTitleClickedMetric_NoDuplicates) {
  // Add tabs with unique titles.
  tabs::TabInterface* tab_a1 = AddTab(GURL("https://a1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(0))
      ->SetTitle(u"Title A");
  AddTab(GURL("https://b1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(1))
      ->SetTitle(u"Title B");

  // Mock the call to GetPageContext.
  MockTabContextualizationController* controller_a1 =
      static_cast<MockTabContextualizationController*>(
          tab_a1->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*controller_a1, GetPageContext(testing::_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(1);
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(1);

  auto* metrics_recorder_ptr = GetMetricsRecorderPtr();
  ASSERT_THAT(metrics_recorder_ptr, testing::NotNull());
  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged(testing::_))
      .WillRepeatedly(testing::Invoke(
          metrics_recorder_ptr,
          &MockContextualSearchMetricsRecorder::NotifySessionStateChangedBase));

  // Click on a tab with a unique title.
  base::test::TestFuture<base::expected<base::UnguessableToken,
                                        contextual_search::FileUploadErrorType>>
      future;
  handler().NotifySessionStarted();
  handler().AddTabContext(tab_a1->GetHandle().raw_value(), false,
                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().has_value());

  // End the session to log the metrics.
  EXPECT_CALL(*GetMetricsRecorderPtr(),
              NotifySessionStateChanged(SessionState::kSessionAbandoned))
      .WillOnce([this](SessionState session_state) {
        GetMetricsRecorderPtr()->NotifySessionStateChangedBase(session_state);
      });
  handler().NotifySessionStateChanged(SessionState::kSessionAbandoned);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabWithDuplicateTitleClicked.V2.NewTabPage", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabContextAdded.V2.NewTabPage", 1, 1);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, TabContextRecencyRankingMetric) {
  // Add tabs with unique titles.
  tabs::TabInterface* tab_a1 = AddTab(GURL("https://a1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(0))
      ->SetTitle(u"Title A");
  AddTab(GURL("https://b1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(1))
      ->SetTitle(u"Title B");

  // Mock the call to GetPageContext.
  MockTabContextualizationController* controller_a1 =
      static_cast<MockTabContextualizationController*>(
          tab_a1->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*controller_a1, GetPageContext(testing::_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  EXPECT_CALL(query_controller(),
              StartFileUploadFlow(testing::_, testing::NotNull(), testing::_))
      .Times(1);
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(1);

  // Click on the first tab.
  base::test::TestFuture<base::expected<base::UnguessableToken,
                                        contextual_search::FileUploadErrorType>>
      future;
  handler().AddTabContext(tab_a1->GetHandle().raw_value(), false,
                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.AddedTabContextRecencyRanking.NewTabPage", 1, 1);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, GetRecentTabs) {
  base::FieldTrialParams params;
  params[ntp_composebox::kContextMenuMaxTabSuggestions.name] = "2";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_composebox::kNtpComposebox, params);

  // Add only 1 valid tab, and ensure it is the only one returned.
  auto* about_blank_tab = AddTab(GURL("about:blank"));
  AddTab(GURL("chrome://webui-is-ignored"));

  base::test::TestFuture<std::vector<searchbox::mojom::TabInfoPtr>> future1;
  handler().GetRecentTabs(future1.GetCallback());
  auto tabs = future1.Take();
  ASSERT_EQ(tabs.size(), 1u);
  EXPECT_EQ(tabs[0]->tab_id, about_blank_tab->GetHandle().raw_value());

  // Add more tabs, and ensure no more than the max allowed tabs are returned.
  AddTab(GURL("https://www.google.com"));
  auto* youtube_tab = AddTab(GURL("https://www.youtube.com"));
  auto* gmail_tab = AddTab(GURL("https://www.gmail.com"));

  base::test::TestFuture<std::vector<searchbox::mojom::TabInfoPtr>> future2;
  handler().GetRecentTabs(future2.GetCallback());
  tabs = future2.Take();
  ASSERT_EQ(tabs.size(), 2u);
  EXPECT_EQ(tabs[0]->tab_id, gmail_tab->GetHandle().raw_value());
  EXPECT_EQ(tabs[1]->tab_id, youtube_tab->GetHandle().raw_value());

  // Activate an older tab, and ensure it is returned first.
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(0))
      ->SetLastActiveTimeTicks(IncrementTimeTicksAndGet());
  base::test::TestFuture<std::vector<searchbox::mojom::TabInfoPtr>> future3;
  handler().GetRecentTabs(future3.GetCallback());
  tabs = future3.Take();
  EXPECT_EQ(tabs[0]->tab_id, about_blank_tab->GetHandle().raw_value());
  EXPECT_EQ(tabs[1]->tab_id, gmail_tab->GetHandle().raw_value());
}

TEST_F(ContextualSearchboxHandlerTestTabsTest,
       GetRecentTabs_SetsShowInRecentTabChip) {
  // Add a regular tab, a google search tab, and another regular tab.
  auto* example_tab = AddTab(GURL("https://www.example.com"));
  auto* search_tab = AddTab(GURL("https://www.google.com/search?q=test"));
  auto* chromium_tab = AddTab(GURL("https://www.chromium.org"));

  // Get the recent tabs.
  base::test::TestFuture<std::vector<searchbox::mojom::TabInfoPtr>> future;
  handler().GetRecentTabs(future.GetCallback());
  auto tabs = future.Take();

  // Expect all three tabs to be returned.
  ASSERT_EQ(tabs.size(), 3u);
  EXPECT_EQ(tabs[0]->tab_id, chromium_tab->GetHandle().raw_value());
  EXPECT_TRUE(tabs[0]->show_in_current_tab_chip);
  EXPECT_FALSE(tabs[0]->show_in_previous_tab_chip);
  EXPECT_EQ(tabs[1]->tab_id, search_tab->GetHandle().raw_value());
  EXPECT_FALSE(tabs[1]->show_in_previous_tab_chip);
  EXPECT_EQ(tabs[2]->tab_id, example_tab->GetHandle().raw_value());
  EXPECT_TRUE(tabs[2]->show_in_previous_tab_chip);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, DuplicateTabsShownMetric) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ContextualSearchboxHandler::kExhaustiveGetRecentTabs);

  // Add tabs with duplicate titles.
  AddTab(GURL("https://a1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(0))
      ->SetTitle(u"Title A");
  AddTab(GURL("https://b1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(1))
      ->SetTitle(u"Title B");
  AddTab(GURL("https://a2.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(2))
      ->SetTitle(u"Title A");
  AddTab(GURL("https://c1.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(3))
      ->SetTitle(u"Title C");
  AddTab(GURL("https://a3.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(4))
      ->SetTitle(u"Title A");
  AddTab(GURL("https://b2.com"));
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(5))
      ->SetTitle(u"Title B");

  base::test::TestFuture<std::vector<searchbox::mojom::TabInfoPtr>>
      tab_info_future;
  handler().GetRecentTabs(tab_info_future.GetCallback());
  auto tabs = tab_info_future.Take();

  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.DuplicateTabTitlesShownCount.NewTabPage", 2, 1);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, ActiveTabsCountMetric) {
  AddTab(GURL("https://a1.com"));
  AddTab(GURL("https://b1.com"));
  AddTab(GURL("https://a2.com"));

  base::test::TestFuture<std::vector<searchbox::mojom::TabInfoPtr>>
      tab_info_future;
  handler().GetRecentTabs(tab_info_future.GetCallback());
  auto tabs = tab_info_future.Take();

  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.ActiveTabsCountOnContextMenuOpen.NewTabPage", 3, 1);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, GetTabPreview_InvalidTab) {
  base::test::TestFuture<const std::optional<std::string>&> future;
  handler().GetTabPreview(12345, future.GetCallback());
  std::optional<std::string> preview = future.Get();
  ASSERT_FALSE(preview.has_value());
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, GetTabPreview_CaptureFails) {
  tabs::TabInterface* tab = AddTab(GURL("https://a1.com"));

  MockTabContextualizationController* controller =
      static_cast<MockTabContextualizationController*>(
          tab->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*controller, CaptureScreenshot(testing::_, testing::_))
      .WillOnce(
          [](std::optional<lens::ImageEncodingOptions> image_options,
             lens::TabContextualizationController::CaptureScreenshotCallback
                 callback) { std::move(callback).Run(SkBitmap()); });

  base::test::TestFuture<const std::optional<std::string>&> future;
  handler().GetTabPreview(tab->GetHandle().raw_value(), future.GetCallback());
  std::optional<std::string> preview = future.Get();
  ASSERT_FALSE(preview.has_value());
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, GetTabPreview_Success) {
  tabs::TabInterface* tab = AddTab(GURL("https://a1.com"));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorRED);

  MockTabContextualizationController* controller =
      static_cast<MockTabContextualizationController*>(
          tab->GetTabFeatures()->tab_contextualization_controller());
  EXPECT_CALL(*controller, CaptureScreenshot(testing::_, testing::_))
      .WillOnce(
          [&bitmap](
              std::optional<lens::ImageEncodingOptions> image_options,
              lens::TabContextualizationController::CaptureScreenshotCallback
                  callback) { std::move(callback).Run(bitmap); });

  base::test::TestFuture<const std::optional<std::string>&> future;
  handler().GetTabPreview(tab->GetHandle().raw_value(), future.GetCallback());
  std::optional<std::string> preview = future.Get();
  ASSERT_TRUE(preview.has_value());
  EXPECT_EQ(preview.value(), webui::GetBitmapDataUrl(bitmap));
}

class ContextualSearchboxHandlerFileUploadStatusTest
    : public ContextualSearchboxHandlerTest,
      public testing::WithParamInterface<
          composebox_query::mojom::FileUploadStatus> {};

TEST_P(ContextualSearchboxHandlerFileUploadStatusTest,
       OnFileUploadStatusChanged) {
  contextual_search::FileUploadStatus status;
  EXPECT_CALL(mock_searchbox_page_, OnContextualInputStatusChanged)
      .Times(1)
      .WillOnce(
          [&status](const base::UnguessableToken& file_token,
                    contextual_search::FileUploadStatus file_upload_status,
                    std::optional<contextual_search::FileUploadErrorType>
                        file_upload_error_type) {
            status = file_upload_status;
          });
  EXPECT_CALL(mock_searchbox_page_, OnInputStateChanged).Times(1);

  const auto expected_status = GetParam();
  contextual_search::FileUploadStatus status_cpp;
  EXPECT_TRUE((mojo::EnumTraits<
               composebox_query::mojom::FileUploadStatus,
               contextual_search::FileUploadStatus>::FromMojom(expected_status,
                                                               &status_cpp)));
  base::UnguessableToken token = base::UnguessableToken::Create();
  handler().OnFileUploadStatusChanged(token, lens::MimeType::kPdf, status_cpp,
                                      std::nullopt);
  mock_searchbox_page_.FlushForTesting();

  EXPECT_EQ(status_cpp, status);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualSearchboxHandlerFileUploadStatusTest,
    testing::Values(
        composebox_query::mojom::FileUploadStatus::kNotUploaded,
        composebox_query::mojom::FileUploadStatus::kProcessing,
        composebox_query::mojom::FileUploadStatus::kValidationFailed,
        composebox_query::mojom::FileUploadStatus::kUploadStarted,
        composebox_query::mojom::FileUploadStatus::kUploadSuccessful,
        composebox_query::mojom::FileUploadStatus::kUploadFailed,
        composebox_query::mojom::FileUploadStatus::kUploadExpired,
        composebox_query::mojom::FileUploadStatus::kUploadReplaced));
