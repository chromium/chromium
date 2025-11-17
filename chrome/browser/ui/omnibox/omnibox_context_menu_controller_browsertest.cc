// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/internal/test_composebox_query_controller.h"
#include "components/lens/contextual_input.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/menus/simple_menu_model.h"

class MockQueryController
    : public contextual_search::TestComposeboxQueryController {
 public:
  MockQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel,
      std::string locale,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      std::unique_ptr<
          contextual_search::ContextualSearchContextController::ConfigParams>
          query_controller_config_params)
      : contextual_search::TestComposeboxQueryController(
            identity_manager,
            url_loader_factory,
            channel,
            locale,
            template_url_service,
            variations_client,
            std::move(query_controller_config_params),
            /*enable_cluster_info_ttl=*/false) {}
  ~MockQueryController() override = default;

  MOCK_METHOD(void, InitializeIfNeeded, (), (override));
  MOCK_METHOD(void,
              StartFileUploadFlow,
              (const base::UnguessableToken& file_token,
               std::unique_ptr<lens::ContextualInputData> contextual_input,
               std::optional<lens::ImageEncodingOptions> image_options),
              (override));
  MOCK_METHOD(bool, DeleteFile, (const base::UnguessableToken&), (override));
  MOCK_METHOD(void, ClearFiles, (), (override));
  MOCK_METHOD(const contextual_search::FileInfo*,
              GetFileInfo,
              (const base::UnguessableToken& file_token),
              (override));

  void InitializeIfNeededBase() {
    TestComposeboxQueryController::InitializeIfNeeded();
  }
};

class MockContextualSearchMetricsRecorder
    : public contextual_search::ContextualSearchMetricsRecorder {
 public:
  MockContextualSearchMetricsRecorder()
      : ContextualSearchMetricsRecorder(
            contextual_search::ContextualSearchSource::kNewTabPage) {}
  ~MockContextualSearchMetricsRecorder() override = default;

  MOCK_METHOD(void,
              NotifySessionStateChanged,
              (contextual_search::SessionState session_state),
              (override));
};

// Override `OpenFileUploadDialog` to track calls.
class TestOmniboxPopupFileSelector : public OmniboxPopupFileSelector {
 public:
  TestOmniboxPopupFileSelector() = default;
  ~TestOmniboxPopupFileSelector() override = default;

  void OpenFileUploadDialog(
      content::WebContents* web_contents,
      bool is_image,
      contextual_search::ContextualSearchContextController* query_controller,
      OmniboxEditModel* edit_model,
      std::optional<lens::ImageEncodingOptions> image_encoding_options)
      override {
    open_file_upload_dialog_calls_++;
  }

  int open_file_upload_dialog_calls() const {
    return open_file_upload_dialog_calls_;
  }

 private:
  int open_file_upload_dialog_calls_ = 0;
};

class OmniboxContextMenuControllerBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxContextMenuControllerBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{omnibox::internal::kWebUIOmniboxAimPopup,
          {{omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
            "inline"},
           {omnibox::kForceToolsAndModels.name, "true"},
           {omnibox::kShowCreateImageTool.name, "true"},
           {omnibox::kShowToolsAndModels.name, "true"}}},
         {omnibox::kWebUIOmniboxPopup, {}}},
        /*disabled_features=*/{omnibox::kAimServerEligibilityEnabled});
  }

  OmniboxContextMenuControllerBrowserTest(
      const OmniboxContextMenuControllerBrowserTest&) = delete;
  OmniboxContextMenuControllerBrowserTest& operator=(
      const OmniboxContextMenuControllerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);

    // Set a default search provider for `template_url_service_`
    template_url_service_ =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
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

    auto query_controller_config_params = std::make_unique<
        contextual_search::ContextualSearchContextController::ConfigParams>();
    query_controller_config_params->send_lns_surface = false;
    query_controller_config_params->enable_multi_context_input_flow = false;
    query_controller_config_params->enable_viewport_images = true;
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, shared_url_loader_factory_,
        version_info::Channel::UNKNOWN, "en-US", template_url_service_,
        fake_variations_client_.get(),
        std::move(query_controller_config_params));
    query_controller_ = query_controller_ptr.get();

    auto metrics_recorder_ptr =
        std::make_unique<MockContextualSearchMetricsRecorder>();
    metrics_recorder_ = metrics_recorder_ptr.get();

    service_ = std::make_unique<contextual_search::ContextualSearchService>(
        /*identity_manager=*/nullptr, shared_url_loader_factory_,
        template_url_service_, fake_variations_client_.get(),
        version_info::Channel::UNKNOWN, "en-US");
    auto contextual_session_handle = service_->CreateSessionForTesting(
        std::move(query_controller_ptr), std::move(metrics_recorder_ptr));
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
        GetWebContents())
        ->set_session_handle(std::move(contextual_session_handle));

    OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
    LocationBar* location_bar = browser()->window()->GetLocationBar();
    OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
        ->set_omnibox_controller(location_bar->GetOmniboxController());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void TearDownOnMainThread() override {
    query_controller_ = nullptr;
    metrics_recorder_ = nullptr;
    service_.reset();
    fake_variations_client_.reset();
    template_url_service_ = nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  raw_ptr<MockQueryController> query_controller_;
  raw_ptr<MockContextualSearchMetricsRecorder> metrics_recorder_;
  std::unique_ptr<contextual_search::ContextualSearchService> service_;
};

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       AddRecentTabsToMenu) {
  auto* web_contents = GetWebContents();
  // TODO(crbug.com/458463536): Use proper web contents for the
  // aim popup.
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>();
  OmniboxContextMenuController base_controller(
      omnibox_popup_file_selector.get(), web_contents);
  ui::SimpleMenuModel* model = base_controller.menu_model();

  // The 1 separator and 4 static items.
  EXPECT_EQ(5u, model->GetItemCount());

  // Navigate the initial tab and add a new one to have exactly two tabs.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_TYPED));

  OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                          web_contents);
  model = controller.menu_model();

  // The model should have 9 items, one for each tab,
  // and 1 header, 2 separators and 4 static items.
  EXPECT_EQ(9u, model->GetItemCount());
}

// TODO(crbug.com/460910010): Flaky, especially on CrOS ASAN LSAN and Win ASAN.
#if defined(ADDRESS_SANITIZER) && \
    ((BUILDFLAG(IS_CHROMEOS) && defined(LEAK_SANITIZER)) || BUILDFLAG(IS_WIN))
#define MAYBE_ExecuteCommand DISABLED_ExecuteCommand
#else
#define MAYBE_ExecuteCommand ExecuteCommand
#endif
IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       MAYBE_ExecuteCommand) {
  TestingPrefServiceSimple pref_service;
  TestOmniboxPopupFileSelector file_selector;
  OmniboxContextMenuController controller(&file_selector, GetWebContents());

  BrowserWindowInterface* browser_window_interface =
      webui::GetBrowserWindowInterface(GetWebContents());
  SearchboxContextData* searchbox_context_data =
      browser_window_interface->GetFeatures().searchbox_context_data();
  ASSERT_TRUE(searchbox_context_data);

  // Test Add Image.
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_ADD_IMAGE, 0);
  EXPECT_EQ(1, file_selector.open_file_upload_dialog_calls());

  // Test Add File.
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_ADD_FILE, 0);
  EXPECT_EQ(2, file_selector.open_file_upload_dialog_calls());

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
          ->get_omnibox_controller();

  // Test Deep Search.
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH, 0);
  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());
  auto context = searchbox_context_data->TakePendingContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(context->mode, searchbox::mojom::ToolMode::kDeepSearch);
  searchbox_context_data->SetPendingContext(std::move(context));

  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kClassic);

  // Test Create Image.
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_CREATE_IMAGES, 0);
  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());
  context = searchbox_context_data->TakePendingContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(context->mode, searchbox::mojom::ToolMode::kCreateImage);
}
