// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"

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
#include "chrome/browser/omnibox/contextual_session_web_contents_helper.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/test_composebox_query_controller.h"
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

using composebox::SessionState;

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
      std::unique_ptr<ComposeboxMetricsRecorder> metrics_recorder,
      std::unique_ptr<OmniboxController> controller)
      : ContextualSearchboxHandler(std::move(pending_page_handler),
                                   profile,
                                   web_contents,
                                   std::move(metrics_recorder),
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
};
}  // namespace

class ContextualSearchboxHandlerTest
    : public ContextualSearchboxHandlerTestHarness {
 public:
  ~ContextualSearchboxHandlerTest() override = default;

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
    handler_ = std::make_unique<FakeContextualSearchboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
        web_contents(), std::move(metrics_recorder_ptr),
        std::make_unique<OmniboxController>(
            /*view=*/nullptr, std::make_unique<TestOmniboxClient>()));

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
  MockComposeboxMetricsRecorder& metrics_recorder() {
    return *metrics_recorder_;
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
  std::unique_ptr<ContextualSessionService> service_;
  raw_ptr<MockComposeboxMetricsRecorder> metrics_recorder_;
  std::unique_ptr<FakeContextualSearchboxHandler> handler_;
};

TEST_F(ContextualSearchboxHandlerTest, SessionStarted) {
  SessionState state_arg = SessionState::kNone;
  EXPECT_CALL(query_controller(), NotifySessionStarted);
  EXPECT_CALL(metrics_recorder(), NotifySessionStateChanged)
      .WillOnce(testing::SaveArg<0>(&state_arg));
  handler().NotifySessionStarted();
  EXPECT_EQ(state_arg, SessionState::kSessionStarted);
}

TEST_F(ContextualSearchboxHandlerTest, SessionAbandoned) {
  SessionState state_arg = SessionState::kNone;
  EXPECT_CALL(query_controller(), NotifySessionAbandoned);
  EXPECT_CALL(metrics_recorder(), NotifySessionStateChanged)
      .WillOnce(testing::SaveArg<0>(&state_arg));
  handler().NotifySessionAbandoned();
  EXPECT_EQ(state_arg, SessionState::kSessionAbandoned);
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
  EXPECT_CALL(query_controller(), ClearFiles);
  handler().ClearFiles();
}

TEST_F(ContextualSearchboxHandlerTest, SubmitQuery) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  query_controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  std::vector<SessionState> session_states;
  EXPECT_CALL(metrics_recorder(), NotifySessionStateChanged)
      .Times(3)
      .WillRepeatedly([&](SessionState session_state) {
        session_states.push_back(session_state);
      });

  // Start the session.
  EXPECT_CALL(query_controller(), NotifySessionStarted)
      .Times(1)
      .WillOnce(testing::Invoke(
          &query_controller(), &MockQueryController::NotifySessionStartedBase));
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
  handler().AddTabContext(sample_tab_id, callback.Get());

  // Flush the mojo pipe to ensure the callback is run.
  mock_searchbox_page_.FlushForTesting();
}

TEST_F(ContextualSearchboxHandlerTestTabsTest, AddTabContextNotFound) {
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  handler().AddTabContext(0, callback.Get());

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
  handler().AddTabContext(tab_id, callback.Get());

  // Check that the histogram was recorded.
  histogram_tester().ExpectUniqueSample("NewTabPage.Composebox.TabContextAdded",
                                        true, 1);
}

TEST_F(ContextualSearchboxHandlerTestTabsTest,
       TabStripModelObserverIsAddedWithValidSession) {
  EXPECT_CALL(mock_searchbox_page_, OnTabStripChanged).Times(1);
  handler().OnTabStripModelChanged(tab_strip_model(), {}, {});
}

TEST_F(ContextualSearchboxHandlerTestTabsTest,
       TabStripModelObserverIsNotAddedWithNullSession) {
  // Create a handler with a null session handle.
  auto handler_with_null_session =
      std::make_unique<FakeContextualSearchboxHandler>(
          mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
          web_contents(), std::make_unique<MockComposeboxMetricsRecorder>(),
          nullptr);

  // Use a new MockSearchboxPage for the new handler.
  testing::NiceMock<MockSearchboxPage> local_mock_searchbox_page;
  handler_with_null_session->SetPage(
      local_mock_searchbox_page.BindAndGetRemote());

  // The observer should not be added, so OnTabStripChanged should not be
  // called.
  EXPECT_CALL(local_mock_searchbox_page, OnTabStripChanged).Times(0);
  handler_with_null_session->OnTabStripModelChanged(tab_strip_model(), {}, {});
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
  handler().AddTabContext(tab_a1->GetHandle().raw_value(), callback1.Get());
  histogram_tester().ExpectUniqueSample(
      "NewTabPage.Composebox.TabWithDuplicateTitleClicked", true, 1);

  // Click on a tab with a unique title.
  base::MockCallback<ComposeboxHandler::AddTabContextCallback> callback2;
  EXPECT_CALL(callback2, Run).Times(1);
  handler().AddTabContext(tab_b1->GetHandle().raw_value(), callback2.Get());
  histogram_tester().ExpectBucketCount(
      "NewTabPage.Composebox.TabWithDuplicateTitleClicked", false, 1);
  histogram_tester().ExpectTotalCount(
      "NewTabPage.Composebox.TabWithDuplicateTitleClicked", 2);
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
  handler().AddTabContext(tab_a1->GetHandle().raw_value(), callback1.Get());
  histogram_tester().ExpectUniqueSample(
      "NewTabPage.Composebox.TabWithDuplicateTitleClicked", false, 1);
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
      "NewTabPage.Composebox.DuplicateTabTitlesShownCount", 2, 1);
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
      "NewTabPage.Composebox.ActiveTabsCountOnContextMenuOpen", 3, 1);
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
  handler().OnFileUploadStatusChanged(token, lens::MimeType::kPdf,
                                      expected_status, std::nullopt);
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
