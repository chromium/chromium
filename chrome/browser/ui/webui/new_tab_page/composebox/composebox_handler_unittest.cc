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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/test_composebox_query_controller.h"
#include "components/variations/variations_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

using composebox::SessionState;

namespace {
constexpr int kImageCompressionQuality = 30;
constexpr int kImageMaxArea = 1000000;
constexpr int kImageMaxHeight = 1000;
constexpr int kImageMaxWidth = 1000;
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

  MOCK_METHOD(void,
              OnContextualInputStatusChanged,
              (const base::UnguessableToken&,
               composebox_query::mojom::FileUploadStatus,
               std::optional<composebox_query::mojom::FileUploadErrorType>));

  mojo::Receiver<composebox::mojom::Page> receiver_{this};
};

class MockTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  explicit MockTabContextualizationController(tabs::TabInterface* tab)
      : lens::TabContextualizationController(tab) {}

  MOCK_METHOD(void,
              GetPageContext,
              (GetPageContextCallback callback),
              (override));
};

}  // namespace

class MockQueryController : public TestComposeboxQueryController {
 public:
  explicit MockQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel,
      std::string locale,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      bool send_lns_surface,
      bool enable_multi_context_input_flow)
      : TestComposeboxQueryController(identity_manager,
                                      url_loader_factory,
                                      channel,
                                      locale,
                                      template_url_service,
                                      variations_client,
                                      send_lns_surface,
                                      enable_multi_context_input_flow) {}
  ~MockQueryController() override = default;

  MOCK_METHOD(void, NotifySessionStarted, ());
  MOCK_METHOD(void, NotifySessionAbandoned, ());
  MOCK_METHOD(void,
              StartFileUploadFlow,
              (const base::UnguessableToken& file_token,
               std::unique_ptr<lens::ContextualInputData> contextual_input,
               std::optional<lens::ImageEncodingOptions> image_options));
  MOCK_METHOD(bool, DeleteFile, (const base::UnguessableToken&));
  MOCK_METHOD(void, ClearFiles, ());
  MOCK_METHOD(FileInfo*,
              GetFileInfo,
              (const base::UnguessableToken& file_token));

  void NotifySessionStartedBase() {
    TestComposeboxQueryController::NotifySessionStarted();
  }
};

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;

  // WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    source->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(params));
    return source;
  }
};

class MockMetricsRecorder : public ComposeboxMetricsRecorder {
 public:
  MockMetricsRecorder() : ComposeboxMetricsRecorder("NewTabPage.") {}
  ~MockMetricsRecorder() override = default;

  MOCK_METHOD(void, NotifySessionStateChanged, (SessionState session_state));
};

class ComposeboxHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  ComposeboxHandlerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ComposeboxHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);

    // Set a default search provider for `template_url_service_`
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(template_url_service_);
    template_url_service_->Load();
    TemplateURLData data;
    data.SetShortName(u"Google");
    data.SetKeyword(u"google.com");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);

    fake_variations_client_ = std::make_unique<FakeVariationsClient>();
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, shared_url_loader_factory_,
        version_info::Channel::UNKNOWN, "en-US", template_url_service_,
        fake_variations_client_.get(), /*send_lns_surface=*/false,
        /*enable_multi_context_input_flow=*/false);
    query_controller_ = query_controller_ptr.get();
    web_contents()->SetDelegate(&delegate_);
    auto metrics_recorder_ptr = std::make_unique<MockMetricsRecorder>();
    metrics_recorder_ = metrics_recorder_ptr.get();
    handler_ = std::make_unique<ComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        std::move(query_controller_ptr), std::move(metrics_recorder_ptr),
        profile(), web_contents(), /*metrics_reporter=*/nullptr);

    handler_->SetPage(mock_searchbox_page_.BindAndGetRemote());
    // Set all the feature params here to keep the test consistent if future
    // default values are changed.

    auto* image_upload = scoped_config_.Get()
                             .config.mutable_composebox()
                             ->mutable_image_upload();
    image_upload->set_downscale_max_image_size(kImageMaxArea);
    image_upload->set_downscale_max_image_width(kImageMaxWidth);
    image_upload->set_downscale_max_image_height(kImageMaxHeight);
    image_upload->set_image_compression_quality(kImageCompressionQuality);
  }

  ComposeboxHandler& handler() { return *handler_; }
  MockQueryController& query_controller() { return *query_controller_; }
  TemplateURLService& template_url_service() { return *template_url_service_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockMetricsRecorder& metrics_recorder() { return *metrics_recorder_; }

  ntp_composebox::FeatureConfig& scoped_config() {
    return scoped_config_.Get();
  }
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return TestingProfile::TestingFactory{
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)};
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
    template_url_service_ = nullptr;
    query_controller_ = nullptr;
    metrics_recorder_ = nullptr;
    handler_.reset();
    fake_variations_client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
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
  ntp_composebox::ScopedFeatureConfigForTesting scoped_config_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  TestWebContentsDelegate delegate_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  raw_ptr<MockQueryController> query_controller_;
  raw_ptr<MockMetricsRecorder> metrics_recorder_;
  std::unique_ptr<ComposeboxHandler> handler_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ComposeboxHandlerTest, SessionStarted) {
  SessionState state_arg = SessionState::kNone;
  EXPECT_CALL(query_controller(), NotifySessionStarted);
  EXPECT_CALL(metrics_recorder(), NotifySessionStateChanged)
      .WillOnce(testing::SaveArg<0>(&state_arg));
  handler().NotifySessionStarted();
  EXPECT_EQ(state_arg, SessionState::kSessionStarted);
}

TEST_F(ComposeboxHandlerTest, SessionAbandoned) {
  SessionState state_arg = SessionState::kNone;
  EXPECT_CALL(query_controller(), NotifySessionAbandoned);
  EXPECT_CALL(metrics_recorder(), NotifySessionStateChanged)
      .WillOnce(testing::SaveArg<0>(&state_arg));
  handler().NotifySessionAbandoned();
  EXPECT_EQ(state_arg, SessionState::kSessionAbandoned);
}

TEST_F(ComposeboxHandlerTest, SubmitQuery) {
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
      .WillRepeatedly(testing::Invoke([&](SessionState session_state) {
        session_states.push_back(session_state);
      }));

  // Start the session.
  EXPECT_CALL(query_controller(), NotifySessionStarted)
      .Times(1)
      .WillOnce(testing::Invoke(
          &query_controller(), &MockQueryController::NotifySessionStartedBase));
  handler().NotifySessionStarted();
  run_loop.Run();

  SubmitQueryAndWaitForNavigation();

  GURL expected_url = query_controller().CreateAimUrl(
      kQueryText, /*query_start_time=*/base::Time::Now());
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

TEST_F(ComposeboxHandlerTest, AddFile_Pdf) {
  composebox::mojom::SelectedFileInfoPtr file_info =
      composebox::mojom::SelectedFileInfo::New();
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

TEST_F(ComposeboxHandlerTest, AddFile_Image) {
  composebox::mojom::SelectedFileInfoPtr file_info =
      composebox::mojom::SelectedFileInfo::New();
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
      .WillOnce(
          testing::Invoke([&token_arg](const base::UnguessableToken& token) {
            token_arg = token;
            return true;
          }));

  EXPECT_CALL(query_controller(), GetFileInfo)
      .WillOnce(
          testing::Invoke([&file_info](const base::UnguessableToken& token) {
            return file_info.get();
          }));

  handler().DeleteContext(delete_file_token);

  SubmitQueryAndWaitForNavigation();

  EXPECT_EQ(delete_file_token, token_arg);
  histogram_tester().ExpectTotalCount(
      kComposeboxFileDeleted + file_type + file_status, 1);
}

TEST_F(ComposeboxHandlerTest, ClearFiles) {
  EXPECT_CALL(query_controller(), ClearFiles);
  handler().ClearFiles();
}

class ComposeboxHandlerTabsTest : public ComposeboxHandlerTest {
 public:
  ComposeboxHandlerTabsTest() = default;

  ~ComposeboxHandlerTabsTest() override {
    // Break loop so we can deconstruct without dangling pointers.
    delegate_.SetBrowserWindowInterface(nullptr);
  }

  void SetUp() override {
    ComposeboxHandlerTest::SetUp();
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
    ComposeboxHandlerTest::TearDown();
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
  base::TimeTicks IncrementTimeTicksAndGet() {
    last_active_time_ticks_ += base::Seconds(1);
    return last_active_time_ticks_;
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
  std::map<tabs::TabInterface* const, std::unique_ptr<tabs::TabAlertController>>
      tab_interface_to_alert_controller_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(ComposeboxHandlerTabsTest, AddTabContext) {
  auto sample_url = GURL("https://www.google.com");
  tabs::TabInterface* tab = AddTab(sample_url);
  const int sample_tab_id = tab->GetHandle().raw_value();

  tabs::TabFeatures* tab_features = tab->GetTabFeatures();
  MockTabContextualizationController* tab_contextualization_controller =
      static_cast<MockTabContextualizationController*>(
          tab_features->tab_contextualization_controller());
  EXPECT_CALL(*tab_contextualization_controller, GetPageContext(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Invoke(
          [](lens::TabContextualizationController::GetPageContextCallback
                 callback) {
            std::move(callback).Run(
                std::make_unique<lens::ContextualInputData>());
          }));

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
  mock_page_.FlushForTesting();
}

TEST_F(ComposeboxHandlerTabsTest, GetRecentTabs) {
  base::FieldTrialParams params;
  params[ntp_composebox::kContextMenuMaxTabSuggestions.name] = "2";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_composebox::kNtpComposebox, params);

  // Add only 1 valid tab, and ensure it is the only one returned.
  auto* about_blank_tab = AddTab(GURL("about:blank"));
  AddTab(GURL("chrome://webui-is-ignored"));

  base::test::TestFuture<std::vector<composebox::mojom::TabInfoPtr>> future1;
  handler().GetRecentTabs(future1.GetCallback());
  auto tabs = future1.Take();
  ASSERT_EQ(tabs.size(), 1u);
  EXPECT_EQ(tabs[0]->tab_id, about_blank_tab->GetHandle().raw_value());

  // Add more tabs, and ensure no more than the max allowed tabs are returned.
  AddTab(GURL("https://www.google.com"));
  auto* youtube_tab = AddTab(GURL("https://www.youtube.com"));
  auto* gmail_tab = AddTab(GURL("https://www.gmail.com"));

  base::test::TestFuture<std::vector<composebox::mojom::TabInfoPtr>> future2;
  handler().GetRecentTabs(future2.GetCallback());
  tabs = future2.Take();
  ASSERT_EQ(tabs.size(), 2u);
  EXPECT_EQ(tabs[0]->tab_id, gmail_tab->GetHandle().raw_value());
  EXPECT_EQ(tabs[1]->tab_id, youtube_tab->GetHandle().raw_value());

  // Activate an older tab, and ensure it is returned first.
  content::WebContentsTester::For(tab_strip_model()->GetWebContentsAt(0))
      ->SetLastActiveTimeTicks(IncrementTimeTicksAndGet());
  base::test::TestFuture<std::vector<composebox::mojom::TabInfoPtr>> future3;
  handler().GetRecentTabs(future3.GetCallback());
  tabs = future3.Take();
  EXPECT_EQ(tabs[0]->tab_id, about_blank_tab->GetHandle().raw_value());
  EXPECT_EQ(tabs[1]->tab_id, gmail_tab->GetHandle().raw_value());
}

TEST_F(ComposeboxHandlerTabsTest, DuplicateTabsShownMetric) {
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

  base::test::TestFuture<std::vector<composebox::mojom::TabInfoPtr>>
      tab_info_future;
  handler().GetRecentTabs(tab_info_future.GetCallback());
  auto tabs = tab_info_future.Take();

  histogram_tester().ExpectUniqueSample(
      "NewTabPage.Composebox.DuplicateTabTitlesShownCount", 2, 1);
}

TEST_F(ComposeboxHandlerTabsTest, ActiveTabsCountMetric) {
  AddTab(GURL("https://a1.com"));
  AddTab(GURL("https://b1.com"));
  AddTab(GURL("https://a2.com"));

  base::test::TestFuture<std::vector<composebox::mojom::TabInfoPtr>>
      tab_info_future;
  handler().GetRecentTabs(tab_info_future.GetCallback());
  auto tabs = tab_info_future.Take();

  histogram_tester().ExpectUniqueSample(
      "NewTabPage.Composebox.ActiveTabsCountOnContextMenuOpen", 3, 1);
}

TEST_F(ComposeboxHandlerTabsTest, TabContextAddedMetric) {
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

TEST_F(ComposeboxHandlerTabsTest, TabWithDuplicateTitleClickedMetric) {
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

TEST_F(ComposeboxHandlerTabsTest,
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

class ComposeboxHandlerFileUploadStatusTest
    : public ComposeboxHandlerTest,
      public testing::WithParamInterface<
          composebox_query::mojom::FileUploadStatus> {};

TEST_P(ComposeboxHandlerFileUploadStatusTest, OnFileUploadStatusChanged) {
  composebox_query::mojom::FileUploadStatus status;
  EXPECT_CALL(mock_page_, OnContextualInputStatusChanged)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&status](
              const base::UnguessableToken& file_token,
              composebox_query::mojom::FileUploadStatus file_upload_status,
              std::optional<composebox_query::mojom::FileUploadErrorType>
                  file_upload_error_type) { status = file_upload_status; }));

  const auto expected_status = GetParam();
  base::UnguessableToken token = base::UnguessableToken::Create();
  handler().OnFileUploadStatusChanged(token, lens::MimeType::kPdf,
                                      expected_status, std::nullopt);
  mock_page_.FlushForTesting();

  EXPECT_EQ(expected_status, status);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ComposeboxHandlerFileUploadStatusTest,
    testing::Values(
        composebox_query::mojom::FileUploadStatus::kNotUploaded,
        composebox_query::mojom::FileUploadStatus::kProcessing,
        composebox_query::mojom::FileUploadStatus::kValidationFailed,
        composebox_query::mojom::FileUploadStatus::kUploadStarted,
        composebox_query::mojom::FileUploadStatus::kUploadSuccessful,
        composebox_query::mojom::FileUploadStatus::kUploadFailed,
        composebox_query::mojom::FileUploadStatus::kUploadExpired));
