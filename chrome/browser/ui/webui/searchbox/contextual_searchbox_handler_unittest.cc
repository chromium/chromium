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
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
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
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
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
      std::unique_ptr<OmniboxController> controller)
      : ContextualSearchboxHandler(std::move(pending_page_handler),
                                   profile,
                                   web_contents,
                                   std::move(controller)) {}
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
    query_controller_config_params->enable_multi_context_input_flow = false;
    query_controller_config_params->enable_viewport_images = true;
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        version_info::Channel::UNKNOWN, "en-US", template_url_service(),
        fake_variations_client(), std::move(query_controller_config_params));
    query_controller_ = query_controller_ptr.get();

    auto metrics_recorder_ptr =
        std::make_unique<MockContextualSearchMetricsRecorder>();

    service_ = std::make_unique<contextual_search::ContextualSearchService>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        template_url_service(), fake_variations_client(),
        version_info::Channel::UNKNOWN, "en-US");
    auto contextual_session_handle = service_->CreateSessionForTesting(
        std::move(query_controller_ptr), std::move(metrics_recorder_ptr));
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents())
        ->set_session_handle(std::move(contextual_session_handle));

    web_contents()->SetDelegate(&delegate_);
    handler_ = std::make_unique<FakeContextualSearchboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
        web_contents(),
        std::make_unique<OmniboxController>(
            std::make_unique<TestOmniboxClient>()));

    handler_->SetPage(mock_searchbox_page_.BindAndGetRemote());
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
    service_.reset();
    ContextualSearchboxHandlerTestHarness::TearDown();
  }

 protected:
  testing::NiceMock<MockSearchboxPage> mock_searchbox_page_;

 private:
  TestWebContentsDelegate delegate_;
  raw_ptr<MockQueryController> query_controller_;
  std::unique_ptr<contextual_search::ContextualSearchService> service_;
  raw_ptr<MockContextualSearchMetricsRecorder> metrics_recorder_;
  std::unique_ptr<FakeContextualSearchboxHandler> handler_;
};

TEST_F(ContextualSearchboxHandlerTest, SessionStarted) {
  SessionState state_arg = SessionState::kNone;
  auto* metrics_recorder_ptr = GetMetricsRecorderPtr();
  ASSERT_THAT(metrics_recorder_ptr, testing::NotNull());

  EXPECT_CALL(query_controller(), InitializeIfNeeded);
  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged)
      .WillOnce(testing::SaveArg<0>(&state_arg));
  handler().NotifySessionStarted();
  EXPECT_EQ(state_arg, SessionState::kSessionStarted);
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
  base::UnguessableToken callback_token;
  EXPECT_CALL(query_controller(), StartFileUploadFlow)
      .WillOnce(testing::SaveArg<0>(&controller_file_info_token));
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&callback_token));
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
  base::UnguessableToken callback_token;
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&callback_token));

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
  base::UnguessableToken callback_token;
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&callback_token));

  handler().AddFileContext(std::move(file_info), std::move(file_data),
                           callback.Get());
  EXPECT_EQ(handler().GetUploadedContextTokensForTesting().size(), 1u);

  EXPECT_CALL(query_controller(), ClearFiles).Times(0);
  handler().ClearFiles();
  EXPECT_EQ(handler().GetUploadedContextTokensForTesting().size(), 0u);
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

  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged)
      .Times(3)
      .WillRepeatedly([&](SessionState session_state) {
        session_states.push_back(session_state);
      });

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
  GURL expected_url =
      query_controller().CreateSearchUrl(std::move(search_url_request_info));
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

  EXPECT_CALL(*metrics_recorder_ptr, NotifySessionStateChanged)
      .Times(3)
      .WillRepeatedly([&](SessionState session_state) {
        session_states.push_back(session_state);
      });

  // Start the session.
  EXPECT_CALL(query_controller(), InitializeIfNeeded)
      .Times(1)
      .WillOnce(testing::Invoke(&query_controller(),
                                &MockQueryController::InitializeIfNeededBase));

  // Set a cached tab context snapshot.
  auto* contextual_search_web_contents_helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents());
  auto token = base::UnguessableToken::Create();
  contextual_search_web_contents_helper->session_handle()
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
  GURL expected_url =
      query_controller().CreateSearchUrl(std::move(search_url_request_info));
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
    ON_CALL(browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(&tab_strip_model_));
    ON_CALL(browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    delegate_.SetBrowserWindowInterface(&browser_window_interface_);
    webui::SetBrowserWindowInterface(web_contents(),
                                     &browser_window_interface_);
  }

  void TearDown() override {
    tab_interface_to_alert_controller_.clear();
    tab_strip_model()->CloseAllTabs();
    ContextualSearchboxHandlerTest::TearDown();
  }

  base::TimeTicks IncrementTimeTicksAndGet() {
    last_active_time_ticks_ += base::Seconds(1);
    return last_active_time_ticks_;
  }

  TestTabStripModelDelegate* delegate() { return &delegate_; }
  TabStripModel* tab_strip_model() { return &tab_strip_model_; }
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
    tab_features->SetTabUIHelperForTesting(
        std::make_unique<TabUIHelper>(*tab_interface));
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
    tab_interface_to_alert_controller_.insert(
        {tab_interface, std::move(tab_alert_controller)});

    return tab_interface;
  }

 private:
  base::TimeTicks last_active_time_ticks_;
  TestTabStripModelDelegate delegate_;
  TabStripModel tab_strip_model_{&delegate_, profile()};
  ui::UnownedUserDataHost user_data_host_;
  MockBrowserWindowInterface browser_window_interface_;
  base::HistogramTester histogram_tester_;
  std::map<tabs::TabInterface* const, std::unique_ptr<tabs::TabAlertController>>
      tab_interface_to_alert_controller_;
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

TEST_F(ContextualSearchboxHandlerTestTabsTest, AddTabContext_DelayUpload) {
  // Arrange
  auto sample_url = GURL("https://www.google.com");
  tabs::TabInterface* tab = AddTab(sample_url);
  const int sample_tab_id = tab->GetHandle().raw_value();

  composebox_query::mojom::FileUploadStatus status;

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
          [&status](
              const base::UnguessableToken& file_token,
              composebox_query::mojom::FileUploadStatus file_upload_status,
              std::optional<composebox_query::mojom::FileUploadErrorType>
                  file_upload_error_type) { status = file_upload_status; });

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
  ASSERT_EQ(composebox_query::mojom::FileUploadStatus::kProcessing, status);
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
  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  auto sample_contextual_input_data =
      std::make_unique<lens::ContextualInputData>();
  sample_contextual_input_data->page_url = sample_url;
  handler().AddTabContext(sample_tab_id, /*delay_upload=*/true,
                          future.GetCallback());
  // Flush the mojo pipe to ensure the callback is run.
  mock_searchbox_page_.FlushForTesting();

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
  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
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

TEST_F(ContextualSearchboxHandlerTestTabsTest, AddTabContextNotFound) {
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  handler().AddTabContext(0, false, callback.Get());

  // Flush the mojo pipe to ensure the callback is run.
  mock_searchbox_page_.FlushForTesting();
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

  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  handler().AddTabContext(tab_id, false, callback.Get());

  // Check that the histogram was recorded.
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabContextAdded.NewTabPage", true, 1);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest,
       TabStripModelObserverIsAddedWithValidSession) {
  EXPECT_CALL(mock_searchbox_page_, OnTabStripChanged).Times(1);
  handler().OnTabStripModelChanged(tab_strip_model(), {}, {});
  mock_searchbox_page_.FlushForTesting();
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
              std::make_unique<TestOmniboxClient>()));

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

  // Click on a tab with a duplicate title.
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback1;
  EXPECT_CALL(callback1, Run).Times(1);
  handler().AddTabContext(tab_a1->GetHandle().raw_value(), false,
                          callback1.Get());
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabWithDuplicateTitleClicked.NewTabPage", true, 1);

  // Click on a tab with a unique title.
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback2;
  EXPECT_CALL(callback2, Run).Times(1);
  handler().AddTabContext(tab_b1->GetHandle().raw_value(), false,
                          callback2.Get());
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.TabWithDuplicateTitleClicked.NewTabPage", false, 1);
  histogram_tester().ExpectTotalCount(
      "ContextualSearch.TabWithDuplicateTitleClicked.NewTabPage", 2);
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

  // Click on a tab with a unique title.
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback1;
  EXPECT_CALL(callback1, Run).Times(1);
  handler().AddTabContext(tab_a1->GetHandle().raw_value(), false,
                          callback1.Get());
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.TabWithDuplicateTitleClicked.NewTabPage", false, 1);
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

  // Click on the first tab.
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback1;
  EXPECT_CALL(callback1, Run).Times(1);
  handler().AddTabContext(tab_a1->GetHandle().raw_value(), false,
                          callback1.Get());
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
  EXPECT_TRUE(tabs[0]->show_in_recent_tab_chip);
  EXPECT_EQ(tabs[1]->tab_id, search_tab->GetHandle().raw_value());
  EXPECT_FALSE(tabs[1]->show_in_recent_tab_chip);
  EXPECT_EQ(tabs[2]->tab_id, example_tab->GetHandle().raw_value());
  EXPECT_TRUE(tabs[2]->show_in_recent_tab_chip);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, DuplicateTabsShownMetric) {
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
  composebox_query::mojom::FileUploadStatus status;
  EXPECT_CALL(mock_searchbox_page_, OnContextualInputStatusChanged)
      .Times(1)
      .WillOnce(
          [&status](
              const base::UnguessableToken& file_token,
              composebox_query::mojom::FileUploadStatus file_upload_status,
              std::optional<composebox_query::mojom::FileUploadErrorType>
                  file_upload_error_type) { status = file_upload_status; });

  const auto expected_status = GetParam();
  base::UnguessableToken token = base::UnguessableToken::Create();
  handler().OnFileUploadStatusChanged(
      token, lens::MimeType::kPdf,
      contextual_search::FromMojom(expected_status), std::nullopt);
  mock_searchbox_page_.FlushForTesting();

  EXPECT_EQ(expected_status, status);
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
        composebox_query::mojom::FileUploadStatus::kUploadExpired));
