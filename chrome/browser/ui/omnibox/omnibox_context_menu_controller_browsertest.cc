// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/test_omnibox_popup_file_selector.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace {

constexpr int kMinOmniboxContextMenuRecentTabsCommandId = 33000;

size_t GetVisibleItemCount(const ui::SimpleMenuModel* menu_model) {
  size_t visible_count = 0;
  for (size_t i = 0; i < menu_model->GetItemCount(); i++) {
    if (menu_model->IsVisibleAt(i)) {
      visible_count++;
    }
  }
  return visible_count;
}

void OpenClassicPopup(Profile* profile, OmniboxController* omnibox_controller) {
  auto* edit_model = omnibox_controller->edit_model();
  edit_model->SetUserText(u"foo");
  AutocompleteInput input(u"foo", metrics::OmniboxEventProto::BLANK,
                          ChromeAutocompleteSchemeClassifier(profile));
  input.set_omit_asynchronous_matches(true);
  omnibox_controller->StartAutocomplete(input);
}

// Dummy bytes representing tab contextual input data in tests.
constexpr uint8_t kDummyTabContextBytes[] = {1, 2, 3};

ui::UserDataFactory::ScopedOverride
CreateMockTabContextualizationControllerOverride() {
  return tabs::TabFeatures::GetUserDataFactoryForTesting()
      .AddOverrideForTesting(base::BindRepeating(
          [](tabs::TabInterface& tab)
              -> std::unique_ptr<lens::TabContextualizationController> {
            auto mock =
                std::make_unique<MockTabContextualizationController>(&tab);
            EXPECT_CALL(*mock, GetPageContext)
                .WillRepeatedly([&tab](lens::TabContextualizationController::
                                           GetPageContextCallback callback) {
                  auto data = std::make_unique<lens::ContextualInputData>();
                  data->is_page_context_eligible = true;
                  data->page_url = tab.GetContents()->GetLastCommittedURL();
                  data->page_title = "Title";
                  data->primary_content_type =
                      lens::MimeType::kAnnotatedPageContent;
                  std::vector<lens::ContextualInput> inputs;
                  inputs.emplace_back(
                      std::vector<uint8_t>(std::begin(kDummyTabContextBytes),
                                           std::end(kDummyTabContextBytes)),
                      lens::MimeType::kAnnotatedPageContent);
                  data->context_input = std::move(inputs);
                  std::move(callback).Run(std::move(data));
                });
            return mock;
          }));
}

}  // namespace

class OmniboxContextMenuControllerBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxContextMenuControllerBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{omnibox::internal::kWebUIOmniboxAimPopup,
          {{omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
            "inline"},
           {omnibox::kShowToolsAndModels.name, "true"}}},
         {omnibox::internal::kWebUIOmniboxPopup, {}},
         {omnibox::kContextManagementInComposebox, {}},
         {omnibox::kContextManagementInOmnibox, {}},
         {contextual_tasks::kContextualTasks, {}}},
        /*disabled_features=*/{omnibox::kAimServerEligibilityEnabled,
                               omnibox::kAimFuseboxEligibilityCheckEnabled,
                               omnibox::kAimUsePecApi});
  }

  OmniboxContextMenuControllerBrowserTest(
      const OmniboxContextMenuControllerBrowserTest&) = delete;
  OmniboxContextMenuControllerBrowserTest& operator=(
      const OmniboxContextMenuControllerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();

    OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
    LocationBar* location_bar =
        BrowserWindow::FromBrowser(browser())->GetLocationBar();
    OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
        ->set_omnibox_controller(location_bar->GetOmniboxController());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  tabs::TabInterface* AddTabToBrowser(int index, const GURL& url) {
    if (!AddTabAtIndexToBrowser(browser(), index, url,
                                ui::PAGE_TRANSITION_TYPED,
                                /*check_navigation_success=*/false)) {
      return nullptr;
    }
    return browser()->tab_strip_model()->GetTabAtIndex(index);
  }

  base::TimeTicks ActivateTabAndGetRecentTime(
      tabs::TabInterface* tab,
      base::TimeTicks previous_time = base::TimeTicks()) {
    int index = browser()->tab_strip_model()->GetIndexOfTab(tab);
    browser()->tab_strip_model()->ActivateTabAt(index);
    auto get_last_active_time = [](tabs::TabInterface* t) {
      content::WebContents* wc = t->GetContents();
      return std::max(wc->GetLastActiveTimeTicks(),
                      wc->GetLastInteractionTimeTicks());
    };
    if (previous_time != base::TimeTicks()) {
      EXPECT_TRUE(base::test::RunUntil(
          [&]() { return get_last_active_time(tab) > previous_time; }));
    }
    return get_last_active_time(tab);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OmniboxInlineTabsContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxInlineTabsContextMenuBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{omnibox::internal::kWebUIOmniboxAimPopup, {}},
         {omnibox::internal::kWebUIOmniboxPopup, {}},
         {omnibox::kContextManagementInComposebox, {}}},
        /*disabled_features=*/{omnibox::kContextManagementInOmnibox});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();

    OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
    LocationBar* location_bar =
        BrowserWindow::FromBrowser(browser())->GetLocationBar();
    OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
        ->set_omnibox_controller(location_bar->GetOmniboxController());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       AddRecentTabsToMenu) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupURL)));

  auto* web_contents = GetWebContents();
  // TODO(crbug.com/458463536): Use proper web contents for the
  // aim popup.
  auto owning_window = gfx::NativeWindow();
  auto omnibox_popup_file_selector = std::make_unique<OmniboxPopupFileSelector>(
      owning_window);
  OmniboxContextMenuController base_controller(
      omnibox_popup_file_selector.get(), web_contents);
  ui::SimpleMenuModel* model = base_controller.menu_model();

  // The model should have the following visible items:
  //   - 2 contextual input items
  //   - 1 separator
  //   - 2 tool input items
  EXPECT_EQ(5u, GetVisibleItemCount(model));

  // Add exactly two additional tabs to the tab strip model.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndex(2, url2, ui::PAGE_TRANSITION_TYPED));

  browser()->tab_strip_model()->ActivateTabAt(0);

  OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                          web_contents);
  model = controller.menu_model();

  // Under the new gated flag, tabs are grouped into a submenu.
  // The main model should contain:
  // - 1 sub-menu item ("Add tabs")
  // - 1 separator
  // - 2 contextual input items
  // - 1 separator
  // - 2 tool input items
  // This totals 7 visible items.
  EXPECT_EQ(7u, GetVisibleItemCount(model));
  ASSERT_TRUE(controller.shared_tabs_menu_model());
  EXPECT_EQ(2u, GetVisibleItemCount(controller.shared_tabs_menu_model()));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NTP_COMPOSEBOX_RECENT_TAB_SUFFIX),
            controller.shared_tabs_menu_model()->GetMinorTextAt(0));
  EXPECT_EQ(std::u16string(),
            controller.shared_tabs_menu_model()->GetMinorTextAt(1));
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       RecentAndCurrentTabLabelsWithFeatureEnabled) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupURL)));
  auto* web_contents = GetWebContents();

  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndex(2, url2, ui::PAGE_TRANSITION_TYPED));

  auto owning_window = gfx::NativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  // Case 1: Active tab is tab 1, which is a tab that can be added
  // as context. Therefore, label it as 'current tab'.
  browser()->tab_strip_model()->ActivateTabAt(1);
  {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            web_contents);
    ASSERT_TRUE(controller.shared_tabs_menu_model());
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_COMPOSE_CURRENT_TAB),
              controller.shared_tabs_menu_model()->GetMinorTextAt(0));
    EXPECT_EQ(std::u16string(),
              controller.shared_tabs_menu_model()->GetMinorTextAt(1));
  }

  // Case 2: Active tab is tab 0 (non-addable tab for context), so tab label
  // should say 'recent tab' instead for other most recent tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            web_contents);
    ASSERT_TRUE(controller.shared_tabs_menu_model());
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NTP_COMPOSEBOX_RECENT_TAB_SUFFIX),
              controller.shared_tabs_menu_model()->GetMinorTextAt(0));
    EXPECT_EQ(std::u16string(),
              controller.shared_tabs_menu_model()->GetMinorTextAt(1));
  }
}

// TODO(crbug.com/460910010): Flaky, especially on ASAN/LSAN bots and certain
// Windows bots.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ExecuteCommand DISABLED_ExecuteCommand
#else
#define MAYBE_ExecuteCommand ExecuteCommand
#endif
IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       MAYBE_ExecuteCommand) {
  TestingPrefServiceSimple pref_service;
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);
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
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       RecordHistogramOnTabSelected) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Navigate the initial tab to the popup URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupURL)));
  auto* popup_web_contents = GetWebContents();

  // Add a recent tab.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url, ui::PAGE_TRANSITION_TYPED));

  // The controller should be associated with the popup web contents.
  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);
  OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                          popup_web_contents);

  // The first recent tab item should be at
  // kMinOmniboxContextMenuRecentTabsCommandId.
  controller.ExecuteCommand(kMinOmniboxContextMenuRecentTabsCommandId, 0);

  histogram_tester.ExpectUniqueSample(
      "ContextualSearch.ContextAdded.ContextAddedMethod.Omnibox", 0, 1);

  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEntrypoint.ClassicPopup.ContextualElement.Clicked",
      omnibox::ContextType::kTab, 1);

  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          "Omnibox.AimEntrypoint.ClassicPopup.ContextualElement.Clicked.Tab"));
}

class OmniboxContextMenuControllerBrowserTestWithCommand
    : public OmniboxContextMenuControllerBrowserTest,
      public testing::WithParamInterface<int> {
 protected:
  int GetCommandId() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(OmniboxContextMenuControllerBrowserTestWithCommand,
                       ExecuteCommand_AiModeOpen_ReopensOnCancel) {
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);
  OmniboxContextMenuController controller(&file_selector, GetWebContents());

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);

  // Start with the popup in AIM state.
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Executing the command should record that AIM was open.
  controller.ExecuteCommand(GetCommandId(), 0);
  EXPECT_TRUE(file_selector.last_was_ai_mode_open());
  EXPECT_EQ(1, file_selector.open_file_upload_dialog_calls());

  // Simulate popup closure that would happen as a result of the dialog opening.
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kNone);

  // Canceling the file selection should restore the AIM state.
  file_selector.FileSelectionCanceled();
  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());
}

IN_PROC_BROWSER_TEST_P(OmniboxContextMenuControllerBrowserTestWithCommand,
                       ExecuteCommand_AiModeClosed_DoesNotReopenOnCancel) {
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);
  OmniboxContextMenuController controller(&file_selector, GetWebContents());

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);

  // Start with the popup in Classic state.
  OpenClassicPopup(browser()->GetProfile(), omnibox_controller);

  // Executing the command should record that AIM was NOT open.
  controller.ExecuteCommand(GetCommandId(), 0);
  EXPECT_FALSE(file_selector.last_was_ai_mode_open());
  EXPECT_EQ(1, file_selector.open_file_upload_dialog_calls());

  // Canceling the file selection should NOT switch to AIM state.
  file_selector.FileSelectionCanceled();
  EXPECT_EQ(OmniboxPopupState::kClassic,
            omnibox_controller->popup_state_manager()->popup_state());
}

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxContextMenuControllerBrowserTestWithCommand,
                         testing::Values(IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
                                         IDC_OMNIBOX_CONTEXT_ADD_FILE));

class OmniboxContextMenuControllerPecBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxContextMenuControllerPecBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{omnibox::internal::kWebUIOmniboxAimPopup,
          {{omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
            "inline"},
           {omnibox::kShowToolsAndModels.name, "true"}}},
         {omnibox::internal::kWebUIOmniboxPopup, {}},
         {omnibox::kAimUsePecApi, {}},
         {omnibox::kForceDriveDisclaimerAccepted, {}},
         {omnibox::kComposeboxDriveContextMenuOption, {}},
         {omnibox::kContextManagementInComposebox, {}},
         {omnibox::kContextManagementInOmnibox, {}}},
        /*disabled_features=*/{omnibox::kAimServerEligibilityEnabled,
                               omnibox::kAimFuseboxEligibilityCheckEnabled});
  }

  OmniboxContextMenuControllerPecBrowserTest(
      const OmniboxContextMenuControllerPecBrowserTest&) = delete;
  OmniboxContextMenuControllerPecBrowserTest& operator=(
      const OmniboxContextMenuControllerPecBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();

    OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
    LocationBar* location_bar =
        BrowserWindow::FromBrowser(browser())->GetLocationBar();
    OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
        ->set_omnibox_controller(location_bar->GetOmniboxController());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       RecordHistogramOnCanvasCommand) {
  base::HistogramTester histogram_tester;

  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);
  OmniboxContextMenuController controller(&file_selector, GetWebContents());

  // Execute the command for canvas. This should not crash even if the
  // composebox_handler is null (which it is by default in this test setup).
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_CANVAS, 0);

  // When AimUsePecApi is enabled and composebox_handler is null, it should
  // fallback to the default logic and log the histogram.
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEntrypoint.ClassicPopup.ContextualElement.Clicked",
      omnibox::ContextType::kCanvas, 1);
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       ExecuteCommandSetsToolMode) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));

  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);
  OmniboxContextMenuController controller(&file_selector, GetWebContents());

  base::HistogramTester histogram_tester;

  auto* web_ui = GetWebContents()->GetWebUI();
  ASSERT_TRUE(web_ui) << "WebContents must have a WebUI";

  auto* web_ui_controller = web_ui->GetController();
  ASSERT_TRUE(web_ui_controller) << "WebUI must have a Controller";

  auto* popup_ui = web_ui_controller->GetAs<OmniboxPopupUI>();
  ASSERT_TRUE(popup_ui) << "Controller must cast to OmniboxPopupUI";

  auto* composebox_handler = popup_ui->composebox_handler();
  ASSERT_TRUE(composebox_handler)
      << "Composebox handler must be initialized for this test!";

  auto* input_state_model = composebox_handler->input_state_model();
  ASSERT_TRUE(input_state_model) << "Input state model must exist!";

  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH, 0);
  EXPECT_EQ(omnibox::TOOL_MODE_DEEP_SEARCH,
            input_state_model->GetInputState().active_tool);
  histogram_tester.ExpectBucketCount("ContextualSearch.Tools.Omnibox",
                                     omnibox::TOOL_MODE_DEEP_SEARCH, 1);

  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_CREATE_IMAGES, 0);
  EXPECT_EQ(omnibox::TOOL_MODE_IMAGE_GEN,
            input_state_model->GetInputState().active_tool);
  histogram_tester.ExpectBucketCount("ContextualSearch.Tools.Omnibox",
                                     omnibox::TOOL_MODE_IMAGE_GEN, 1);

  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_CANVAS, 0);
  EXPECT_EQ(omnibox::TOOL_MODE_CANVAS,
            input_state_model->GetInputState().active_tool);
  histogram_tester.ExpectBucketCount("ContextualSearch.Tools.Omnibox",
                                     omnibox::TOOL_MODE_CANVAS, 1);
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       ExecuteCommandRecordsModelMetrics) {
  base::HistogramTester histogram_tester;

  // Navigate to the AIM page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));

  auto* web_contents = GetWebContents();
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);

  // Manually inject InputState to ensure models appear in the menu.
  auto* web_ui = web_contents->GetWebUI();
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  auto* handler = popup_ui->composebox_handler();

  omnibox::InputState test_state;
  // Explicitly allow Gemini Pro and Auto models.
  test_state.allowed_models.emplace_back(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  test_state.allowed_models.emplace_back(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
  handler->input_state_model()->set_state_for_testing(test_state);

  // Create the controller after the state has been injected.
  OmniboxContextMenuController controller(&file_selector, web_contents);
  ui::SimpleMenuModel* menu_model = controller.menu_model();

  // Get the localized labels for the models.
  std::u16string thinking_label =
      l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_THINKING_3_PRO);
  std::u16string auto_label =
      l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_AUTO_MODEL);

  int thinking_model_cmd_id = -1;
  int auto_model_cmd_id = -1;

  // Iterate through the menu to find the dynamic command IDs for the models.
  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    std::u16string label = menu_model->GetLabelAt(i);
    if (label == thinking_label) {
      thinking_model_cmd_id = menu_model->GetCommandIdAt(i);
    } else if (label == auto_label) {
      auto_model_cmd_id = menu_model->GetCommandIdAt(i);
    }
  }

  // Verify and execute the "Thinking" model click.
  ASSERT_NE(thinking_model_cmd_id, -1) << "Thinking model not found in menu";
  controller.ExecuteCommand(thinking_model_cmd_id, 0);
  histogram_tester.ExpectBucketCount("ContextualSearch.Models.Omnibox",
                                     omnibox::ModelMode::MODEL_MODE_GEMINI_PRO,
                                     1);

  // Verify and execute the "Auto" model click.
  ASSERT_NE(auto_model_cmd_id, -1) << "Auto model not found in menu";
  controller.ExecuteCommand(auto_model_cmd_id, 0);
  histogram_tester.ExpectBucketCount(
      "ContextualSearch.Models.Omnibox",
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE, 1);
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       ExecuteCommandRecordsToolMetricsPec) {
  base::HistogramTester histogram_tester;

  // Navigate to the AIM page to initialize the environment.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));

  auto* web_contents = GetWebContents();
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);

  // Manually inject InputState to ensure tools are populated in the menu.
  auto* web_ui = web_contents->GetWebUI();
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  auto* handler = popup_ui->composebox_handler();

  omnibox::InputState test_state;
  test_state.allowed_tools.emplace_back(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  test_state.allowed_tools.emplace_back(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  handler->input_state_model()->set_state_for_testing(test_state);

  // Create the controller after the state is injected.
  OmniboxContextMenuController controller(&file_selector, web_contents);
  ui::SimpleMenuModel* menu_model = controller.menu_model();

  // Define the tools and their corresponding localized label IDs to verify.
  struct {
    int string_id;
    omnibox::ToolMode expected_mode;
  } tool_cases[] = {
      {IDS_NTP_COMPOSE_DEEP_SEARCH, omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH},
      {IDS_NTP_COMPOSE_CREATE_IMAGES, omnibox::ToolMode::TOOL_MODE_IMAGE_GEN}};

  // Iterate through and verify each tool.
  for (const auto& test : tool_cases) {
    std::u16string target_label = l10n_util::GetStringUTF16(test.string_id);
    int command_id = -1;

    // Find the tool's command ID from the dynamically generated menu.
    for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
      if (menu_model->GetLabelAt(i) == target_label) {
        command_id = menu_model->GetCommandIdAt(i);
        break;
      }
    }

    // Execute the command and verify the metrics.
    ASSERT_NE(command_id, -1) << "Tool not found in menu: " << target_label;

    controller.ExecuteCommand(command_id, 0);

    // Verify the 'ContextualSearch.Tools.Omnibox' histogram.
    histogram_tester.ExpectBucketCount("ContextualSearch.Tools.Omnibox",
                                       test.expected_mode, 1);
  }
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       ExecuteCommand_DriveOption_OnSelection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));

  auto* web_contents = GetWebContents();
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);

  auto* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  ASSERT_TRUE(popup_ui);
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);
  ASSERT_TRUE(handler->input_state_model());

  omnibox::InputState test_state;
  test_state.allowed_input_types.emplace_back(
      omnibox::InputType::INPUT_TYPE_DRIVE);
  test_state.max_total_inputs = 5;
  handler->input_state_model()->set_state_for_testing(test_state);

  OmniboxContextMenuController controller(&file_selector, web_contents);
  ui::SimpleMenuModel* menu_model = controller.menu_model();

  std::u16string target_label =
      l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_DRIVE);
  int command_id = -1;

  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    if (menu_model->GetLabelAt(i) == target_label) {
      command_id = menu_model->GetCommandIdAt(i);
      break;
    }
  }

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  ASSERT_NE(command_id, -1) << "Drive option not found in menu";
  controller.ExecuteCommand(command_id, 0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());

  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kNone);

  std::vector<drive_picker_host::mojom::DriveFilePtr> files;
  auto file = drive_picker_host::mojom::DriveFile::New();
  file->id = "valid-id";
  file->name = "test.png";
  file->mime_type = "image/png";
  file->type = "photo";
  file->size_bytes = 1000;
  files.emplace_back(std::move(file));

  handler->OnSelection(std::move(files));

  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());

  auto* session_handle = popup_ui->GetOrCreateContextualSessionHandle();
  ASSERT_TRUE(session_handle);
  auto uploaded_files = session_handle->GetUploadedContextFileInfos();
  ASSERT_EQ(1u, uploaded_files.size());
  EXPECT_EQ("test.png", uploaded_files[0].file_name);
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       ExecuteCommand_DriveOption_Aim_OnCancel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));

  auto* web_contents = GetWebContents();
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);

  auto* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  ASSERT_TRUE(popup_ui);
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);
  ASSERT_TRUE(handler->input_state_model());

  omnibox::InputState test_state;
  test_state.allowed_input_types.emplace_back(
      omnibox::InputType::INPUT_TYPE_DRIVE);
  handler->input_state_model()->set_state_for_testing(test_state);

  OmniboxContextMenuController controller(&file_selector, web_contents);
  ui::SimpleMenuModel* menu_model = controller.menu_model();

  std::u16string target_label =
      l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_DRIVE);
  int command_id = -1;

  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    if (menu_model->GetLabelAt(i) == target_label) {
      command_id = menu_model->GetCommandIdAt(i);
      break;
    }
  }

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  ASSERT_NE(command_id, -1) << "Drive option not found in menu";
  controller.ExecuteCommand(command_id, 0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());

  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kNone);

  handler->OnCancel();

  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       ExecuteCommand_DriveOption_Classic_OnCancel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));

  auto* web_contents = GetWebContents();
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);

  auto* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  ASSERT_TRUE(popup_ui);
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);
  ASSERT_TRUE(handler->input_state_model());

  omnibox::InputState test_state;
  test_state.allowed_input_types.emplace_back(
      omnibox::InputType::INPUT_TYPE_DRIVE);
  handler->input_state_model()->set_state_for_testing(test_state);

  OmniboxContextMenuController controller(&file_selector, web_contents);
  ui::SimpleMenuModel* menu_model = controller.menu_model();

  std::u16string target_label =
      l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_DRIVE);
  int command_id = -1;

  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    if (menu_model->GetLabelAt(i) == target_label) {
      command_id = menu_model->GetCommandIdAt(i);
      break;
    }
  }

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  OpenClassicPopup(browser()->GetProfile(), omnibox_controller);

  ASSERT_NE(command_id, -1) << "Drive option not found in menu";
  controller.ExecuteCommand(command_id, 0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(OmniboxPopupState::kClassic,
            omnibox_controller->popup_state_manager()->popup_state());

  handler->OnCancel();

  EXPECT_EQ(OmniboxPopupState::kClassic,
            omnibox_controller->popup_state_manager()->popup_state());
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       ExecuteCommand_DriveOption_OnError) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));

  auto* web_contents = GetWebContents();
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);

  auto* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  ASSERT_TRUE(popup_ui);
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);
  ASSERT_TRUE(handler->input_state_model());

  omnibox::InputState test_state;
  test_state.allowed_input_types.emplace_back(
      omnibox::InputType::INPUT_TYPE_DRIVE);
  handler->input_state_model()->set_state_for_testing(test_state);

  OmniboxContextMenuController controller(&file_selector, web_contents);
  ui::SimpleMenuModel* menu_model = controller.menu_model();

  std::u16string target_label =
      l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_DRIVE);
  int command_id = -1;

  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    if (menu_model->GetLabelAt(i) == target_label) {
      command_id = menu_model->GetCommandIdAt(i);
      break;
    }
  }

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  ASSERT_NE(command_id, -1) << "Drive option not found in menu";
  controller.ExecuteCommand(command_id, 0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());

  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kNone);

  handler->OnError(drive_picker_host::mojom::DrivePickerError::kWindowNotFound);

  EXPECT_EQ(OmniboxPopupState::kAim,
            omnibox_controller->popup_state_manager()->popup_state());
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       ModelPickerCheckmark) {
  // Navigate the initial tab to the popup URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));
  auto* web_contents = GetWebContents();

  auto check_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
      ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);

  // Set the popup state to composebox AIM so that session handle and composebox
  // handler are active.
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Configure active and allowed AI models.
  omnibox::InputState input_state;
  input_state.allowed_models.emplace_back(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  input_state.allowed_models.emplace_back(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI);
  input_state.active_model =
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;

  // Set up the context menu with the allowed AI models in input state.
  auto* web_ui = web_contents->GetWebUI();
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);
  ASSERT_TRUE(handler->input_state_model());
  handler->input_state_model()->set_state_for_testing(input_state);

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                          web_contents);

  ui::SimpleMenuModel* model = controller.menu_model();

  // Find command ID for Pro AI mode.
  int pro_command_id = -1;
  for (const auto& pair : controller.model_for_command_id_) {
    if (pair.second == omnibox::ModelMode::MODEL_MODE_GEMINI_PRO) {
      pro_command_id = pair.first;
      break;
    }
  }
  ASSERT_NE(pro_command_id, -1);

  // Find command ID for Fast AI mode.
  int fast_command_id = -1;
  for (const auto& pair : controller.model_for_command_id_) {
    if (pair.second == omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI) {
      fast_command_id = pair.first;
      break;
    }
  }
  ASSERT_NE(fast_command_id, -1);

  // Verify AI Fast mode checkmark is on the right hand side.
  {
    std::optional<size_t> index = model->GetIndexOfCommandId(fast_command_id);
    ASSERT_TRUE(index.has_value());
    // Checkmark is not empty.
    EXPECT_FALSE(model->GetMinorIconAt(index.value()).IsEmpty());
    // LHS icon is still the model icon, not the checkmark.
    EXPECT_FALSE(model->GetIconAt(index.value()).IsEmpty());
    EXPECT_NE(model->GetIconAt(index.value()), check_icon);
  }

  // Verify AI Pro checkmark is not shown.
  {
    std::optional<size_t> index = model->GetIndexOfCommandId(pro_command_id);
    ASSERT_TRUE(index.has_value());
    EXPECT_TRUE(model->GetMinorIconAt(index.value()).IsEmpty());
    // LHS icon is still the model icon.
    EXPECT_FALSE(model->GetIconAt(index.value()).IsEmpty());
    EXPECT_NE(model->GetIconAt(index.value()), check_icon);
  }

  // Assert internal browser process state is set to Fast mode.
  EXPECT_EQ(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI,
            handler->input_state_model()->GetInputState().active_model);

  // Select AI Pro model.
  controller.ExecuteCommand(pro_command_id, 0);

  // Verify the state is changed in the browser process internal state.
  {
    EXPECT_EQ(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO,
              handler->input_state_model()->GetInputState().active_model);

    // Recreate controller to build menu with updated state.
    OmniboxContextMenuController new_controller(
        omnibox_popup_file_selector.get(), web_contents);
    ui::SimpleMenuModel* new_model = new_controller.menu_model();

    // Verify the checkmark exists for AI Pro mode.
    std::optional<size_t> pro_index =
        new_model->GetIndexOfCommandId(pro_command_id);
    ASSERT_TRUE(pro_index.has_value());
    EXPECT_FALSE(new_model->GetMinorIconAt(pro_index.value()).IsEmpty());
    // LHS icon is still the model icon.
    EXPECT_FALSE(new_model->GetIconAt(pro_index.value()).IsEmpty());
    EXPECT_NE(new_model->GetIconAt(pro_index.value()), check_icon);

    // Verify the checkmark does not exist for AI Fast mode.
    std::optional<size_t> index =
        new_model->GetIndexOfCommandId(fast_command_id);
    ASSERT_TRUE(index.has_value());
    EXPECT_TRUE(new_model->GetMinorIconAt(index.value()).IsEmpty());
    // LHS icon is still the model icon.
    EXPECT_FALSE(new_model->GetIconAt(index.value()).IsEmpty());
    EXPECT_NE(new_model->GetIconAt(index.value()), check_icon);

    // Select AI Fast Model.
    new_controller.ExecuteCommand(fast_command_id, 0);
  }

  // Verify the state is changed in the internal state of the browser process.
  {
    EXPECT_EQ(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI,
              handler->input_state_model()->GetInputState().active_model);

    // Recreate controller to update UI.
    OmniboxContextMenuController final_controller(
        omnibox_popup_file_selector.get(), web_contents);
    ui::SimpleMenuModel* final_model = final_controller.menu_model();

    // Verify the checkmark exists for AI Fast mode.
    std::optional<size_t> index =
        final_model->GetIndexOfCommandId(fast_command_id);
    ASSERT_TRUE(index.has_value());
    EXPECT_FALSE(final_model->GetMinorIconAt(index.value()).IsEmpty());
    // LHS icon is still the model icon.
    EXPECT_FALSE(final_model->GetIconAt(index.value()).IsEmpty());
    EXPECT_NE(final_model->GetIconAt(index.value()), check_icon);

    // Verify the checkmark does not exist for AI Pro mode.
    std::optional<size_t> pro_index =
        final_model->GetIndexOfCommandId(pro_command_id);
    ASSERT_TRUE(pro_index.has_value());
    EXPECT_TRUE(final_model->GetMinorIconAt(pro_index.value()).IsEmpty());
    // LHS icon is still the model icon.
    EXPECT_FALSE(final_model->GetIconAt(pro_index.value()).IsEmpty());
    EXPECT_NE(final_model->GetIconAt(pro_index.value()), check_icon);
  }
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       RecentTabsCheckmarkToggle) {
  // Navigate the initial tab to the popup URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));
  auto* web_contents = GetWebContents();

  // Set the popup state to composebox so that session handle and
  // composebox handler are active.
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Get the test URL.
  GURL url(embedded_test_server()->GetURL("/title1.html"));

  // Set up an override to construct `MockTabContextualizationController`
  // to mock the context of the tab.
  ui::UserDataFactory::ScopedOverride controller_override =
      CreateMockTabContextualizationControllerOverride();

  // Add a recent tab (created with the mock controller) in the background.
  ASSERT_TRUE(AddTabToBrowser(1, url));

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  // Initial State: Tab is not added (unchecked).
  {
    auto* web_ui = web_contents->GetWebUI();
    auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
    auto* handler = popup_ui->composebox_handler();
    ASSERT_TRUE(handler);
    EXPECT_TRUE(handler->selected_tabs.empty());

    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            web_contents);

    ui::SimpleMenuModel* target_model =
        controller.shared_tabs_menu_model()
            ? controller.shared_tabs_menu_model()
            : controller.menu_model();

    // Find the recent tab item in the menu.
    std::optional<size_t> index = target_model->GetIndexOfCommandId(
        kMinOmniboxContextMenuRecentTabsCommandId);
    ASSERT_TRUE(index.has_value());

    // Verify that the minor icon is empty (not checked).
    EXPECT_TRUE(target_model->GetMinorIconAt(index.value()).IsEmpty());
  }

  // Select the tab -> This should add the tab to the context (stage it).
  {
    tabs::TabInterface* tab = browser()->tab_strip_model()->GetTabAtIndex(1);
    ASSERT_TRUE(tab);
    int32_t tab_id = tab->GetHandle().raw_value();

    auto* web_ui = web_contents->GetWebUI();
    auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
    auto* handler = popup_ui->composebox_handler();
    ASSERT_TRUE(handler);

    // Stage the tab directly via C++ handler.
    handler->AddTabContext(tab_id, /*delay_upload=*/false,
                           searchbox::mojom::TabAttachmentSource::kContextMenu,
                           base::DoNothing());

    // Verify the tab is staged for upload in C++ tracking.
    EXPECT_EQ(1u, handler->selected_tabs.size());
    EXPECT_EQ(tab_id, handler->selected_tabs.begin()->second);
  }

  // Verify the tab is now checked (has minor icon).
  {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            web_contents);

    ui::SimpleMenuModel* target_model =
        controller.shared_tabs_menu_model()
            ? controller.shared_tabs_menu_model()
            : controller.menu_model();

    std::optional<size_t> index = target_model->GetIndexOfCommandId(
        kMinOmniboxContextMenuRecentTabsCommandId);
    ASSERT_TRUE(index.has_value());

    // Verify that the minor icon is not empty (it has the checkmark),
    EXPECT_FALSE(target_model->GetMinorIconAt(index.value()).IsEmpty());
  }

  // Select the tab again -> This should remove/uncheck it from the context.
  {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            web_contents);
    // Execute command on already checked tab should toggle it off
    controller.ExecuteCommand(kMinOmniboxContextMenuRecentTabsCommandId, 0);
  }

  // Verify the tab is now unchecked again.
  {
    auto* web_ui = web_contents->GetWebUI();
    auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
    auto* handler = popup_ui->composebox_handler();
    ASSERT_TRUE(handler);
    // Verify the tab is removed from C++ tracking for staged uploads.
    EXPECT_TRUE(handler->selected_tabs.empty());

    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            web_contents);

    ui::SimpleMenuModel* target_model =
        controller.shared_tabs_menu_model()
            ? controller.shared_tabs_menu_model()
            : controller.menu_model();

    std::optional<size_t> index = target_model->GetIndexOfCommandId(
        kMinOmniboxContextMenuRecentTabsCommandId);
    ASSERT_TRUE(index.has_value());

    // Verify that the minor icon is empty again.
    EXPECT_TRUE(target_model->GetMinorIconAt(index.value()).IsEmpty());
  }
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       SortTabsWithCheckedFirst) {
  // Navigate the initial tab to the popup URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));
  auto* web_contents = GetWebContents();

  // Set the popup state to composebox (AIM) so that session handle and
  // composebox handler are active.
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Set up an override to construct `MockTabContextualizationController`
  // for all tabs in this test.
  ui::UserDataFactory::ScopedOverride controller_override =
      CreateMockTabContextualizationControllerOverride();

  // Add three tabs.
  tabs::TabInterface* tab1 =
      AddTabToBrowser(1, embedded_test_server()->GetURL("/title2.html"));
  tabs::TabInterface* tab2 =
      AddTabToBrowser(2, embedded_test_server()->GetURL("/title3.html"));
  tabs::TabInterface* tab3 =
      AddTabToBrowser(3, embedded_test_server()->GetURL("/simple.html"));
  ASSERT_TRUE(tab1 && tab2 && tab3);

  // Order of activation/recency: 3, then 2, then 1 (making 1 active/most
  // recent).
  base::TimeTicks t3 = ActivateTabAndGetRecentTime(tab3);
  base::TimeTicks t2 = ActivateTabAndGetRecentTime(tab2, t3);
  ActivateTabAndGetRecentTime(tab1, t2);

  int32_t tab2_id = tab2->GetHandle().raw_value();
  int32_t tab3_id = tab3->GetHandle().raw_value();

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  auto* web_ui = web_contents->GetWebUI();
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);

  // Helper lambda to get tab items from the menu.
  auto get_tab_items = [&]() {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            web_contents);
    std::vector<std::pair<std::u16string, bool>> items;
    ui::SimpleMenuModel* target_model =
        controller.shared_tabs_menu_model()
            ? controller.shared_tabs_menu_model()
            : controller.menu_model();

    for (size_t i = 0; i < target_model->GetItemCount(); ++i) {
      int command_id = target_model->GetCommandIdAt(i);
      if (controller.IsTabCommandId(command_id)) {
        bool has_checkmark = !target_model->GetMinorIconAt(i).IsEmpty();
        items.emplace_back(target_model->GetLabelAt(i), has_checkmark);
      }
    }
    return items;
  };

  // Initially, all tabs are unchecked. Sorting should be based on
  // recency/active. Tab 1 is currently active/most recent.
  {
    auto tab_items = get_tab_items();
    ASSERT_EQ(3u, tab_items.size());
    EXPECT_EQ(tab_items[0].first, u"Title Of Awesomeness");  // Tab 1
    EXPECT_FALSE(tab_items[0].second);
    EXPECT_EQ(tab_items[1].first, u"Title Of More Awesomeness");  // Tab 2
    EXPECT_FALSE(tab_items[1].second);
    EXPECT_EQ(tab_items[2].first, u"OK");  // Tab 3
    EXPECT_FALSE(tab_items[2].second);
  }

  // Click on tab 3, making its checkmark appear.
  handler->AddTabContext(tab3_id, /*delay_upload=*/false,
                         searchbox::mojom::TabAttachmentSource::kContextMenu,
                         base::DoNothing());

  // Now, Tab 3 should be sorted second since tab 2
  // is also checked (selected) but more recent than tab 3. Tab 1 is most
  // recent, but it is not checked (not selected), so it goes after the checked
  // tabs.
  {
    auto tab_items = get_tab_items();
    ASSERT_EQ(3u, tab_items.size());
    // Tab 2: Checked and more recent:
    EXPECT_EQ(tab_items[2].first, u"Title Of More Awesomeness");
    EXPECT_FALSE(tab_items[2].second);
    // Tab 3 (checked): Selected and less recent:
    EXPECT_EQ(tab_items[0].first, u"OK");  // Tab 3 (checked)
    EXPECT_TRUE(tab_items[0].second);
    // Tab 1 (unchecked): Most recent but unselected:
    EXPECT_EQ(tab_items[1].first, u"Title Of Awesomeness");
    EXPECT_FALSE(tab_items[1].second);
  }

  // Stage (check/select) Tab 2 for upload as well.
  handler->AddTabContext(tab2_id, /*delay_upload=*/false,
                         searchbox::mojom::TabAttachmentSource::kContextMenu,
                         base::DoNothing());

  // Now, both Tab 2 and Tab 3 are checked.
  // Tab 2 is more recent than Tab 3, so order should be: Tab 2, Tab 3, Tab 1.
  {
    auto tab_items = get_tab_items();
    ASSERT_EQ(3u, tab_items.size());
    // Tab 2 (checked, more recent):
    EXPECT_EQ(tab_items[0].first, u"Title Of More Awesomeness");
    EXPECT_TRUE(tab_items[0].second);
    // Tab 3 (checked, less recent):
    EXPECT_EQ(tab_items[1].first, u"OK");
    EXPECT_TRUE(tab_items[1].second);
    // Tab 1 (unchecked):
    EXPECT_EQ(tab_items[2].first, u"Title Of Awesomeness");
    EXPECT_FALSE(tab_items[2].second);
  }

  // Delete Tab 2 from staged tabs.
  // Find token of Tab 2.
  base::UnguessableToken tab2_token;
  for (const auto& pair : handler->selected_tabs) {
    if (pair.second == tab2_id) {
      tab2_token = pair.first;
      break;
    }
  }
  ASSERT_FALSE(tab2_token.is_empty());
  handler->DeleteContextFromBrowser(tab2_token, /*from_automatic_chip=*/false);

  // Now only Tab 3 is checked. Order should be: Tab 3, Tab 1, Tab 2.
  {
    auto tab_items = get_tab_items();
    ASSERT_EQ(3u, tab_items.size());
    // Tab 3 (checked):
    EXPECT_EQ(tab_items[0].first, u"OK");
    EXPECT_TRUE(tab_items[0].second);
    // Tab 1 (unchecked, more recent):
    EXPECT_EQ(tab_items[1].first, u"Title Of Awesomeness");
    EXPECT_FALSE(tab_items[1].second);
    // Tab 2 (unchecked, less recent):
    EXPECT_EQ(tab_items[2].first, u"Title Of More Awesomeness");
    EXPECT_FALSE(tab_items[2].second);
  }
}

class TestActiveTaskContextProviderObserver
    : public contextual_tasks::ActiveTaskContextProvider::Observer {
 public:
  void OnContextTabsChanged(
      const std::set<tabs::TabHandle>& context_tabs) override {
    last_context_tabs_ = context_tabs;
  }
  const std::set<tabs::TabHandle>& last_context_tabs() const {
    return last_context_tabs_;
  }

 private:
  std::set<tabs::TabHandle> last_context_tabs_;
};

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       UnderlinesNotClearedOnOtherTabsOpeningOrClosing) {
  // Start with tab 1 (index 0) active.
  tabs::TabInterface* tab1 = browser()->tab_strip_model()->GetTabAtIndex(0);

  // Add tab 2 (index 1) with a normal web URL.
  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, url2,
                                     ui::PAGE_TRANSITION_TYPED,
                                     /*check_navigation_success=*/false));
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Tab 1 is active.
  ASSERT_EQ(tab1, browser()->tab_strip_model()->GetActiveTab());

  tabs::TabInterface* tab2 = browser()->tab_strip_model()->GetTabAtIndex(1);

  // Register test observer.
  auto* active_task_context_provider =
      contextual_tasks::ActiveTaskContextProvider::From(browser());
  ASSERT_TRUE(active_task_context_provider);
  TestActiveTaskContextProviderObserver observer;
  active_task_context_provider->AddObserver(&observer);
  base::ScopedClosureRunner remove_observer(base::BindOnce(
      &contextual_tasks::ActiveTaskContextProvider::RemoveObserver,
      base::Unretained(active_task_context_provider), &observer));

  // Add tab 2 context directly to tab 1's local underlines (since tab 1 is
  // active).
  active_task_context_provider->AddLocalTabUnderline(tab2->GetHandle());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return observer.last_context_tabs().contains(tab2->GetHandle());
  }));

  // Open tab 3 with a normal web URL.
  GURL url3(embedded_test_server()->GetURL("/title3.html"));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 2, url3,
                                     ui::PAGE_TRANSITION_TYPED,
                                     /*check_navigation_success=*/false));
  tabs::TabInterface* tab3 = browser()->tab_strip_model()->GetTabAtIndex(2);

  // Ensure tab 1 is active, then add tab 3 to tab 1's local underlines list.
  browser()->tab_strip_model()->ActivateTabAt(0);
  active_task_context_provider->AddLocalTabUnderline(tab3->GetHandle());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return observer.last_context_tabs().contains(tab2->GetHandle()) &&
           observer.last_context_tabs().contains(tab3->GetHandle());
  }));

  // Switch active tab to tab 3.
  browser()->tab_strip_model()->ActivateTabAt(2);
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Close tab 3, removing it from tab 1's underlines.
  browser()->tab_strip_model()->CloseWebContentsAt(
      2, TabCloseTypes::CLOSE_USER_GESTURE);

  // Switch back to tab 1 (index 0).
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Verify that tab 2 is still in the local underlines of tab 1, instead of
  // being deselected (due to tab 3 being deselected) like in previous bugs.
  EXPECT_TRUE(observer.last_context_tabs().contains(tab2->GetHandle()));
}

class OmniboxContextMenuControllerPecBrowserTestWithFlagsDisabled
    : public OmniboxContextMenuControllerPecBrowserTest {
 public:
  OmniboxContextMenuControllerPecBrowserTestWithFlagsDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{omnibox::kContextManagementInComposebox,
                               omnibox::kContextManagementInOmnibox});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that when the flags are disabled, the recent tabs list is rendered
// directly in the main menu, and no flying out submenu is created.
IN_PROC_BROWSER_TEST_F(
    OmniboxContextMenuControllerPecBrowserTestWithFlagsDisabled,
    VerifyNoFlyoutMenu_FlagOff) {
  // Navigate to the target WebUI URL where the contextual handler is hosted.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));
  auto* web_contents = GetWebContents();

  // Force the popup state manager into AIM mode to mimic active UI conditions.
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Initialize and inject an InputState mock that explicitly authorizes tab
  // suggestions.
  omnibox::InputState input_state;
  input_state.allowed_input_types.push_back(
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  input_state.max_inputs_by_type[omnibox::InputType::INPUT_TYPE_BROWSER_TAB] =
      5;

  auto* web_ui = web_contents->GetWebUI();
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);
  ASSERT_TRUE(handler->input_state_model());
  handler->input_state_model()->set_state_for_testing(input_state);

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  // Seed an independent browser tab to evaluate flat menu rendering behavior.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  // Instantiate the controller which triggers menu hierarchy construction.
  OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                          web_contents);
  ui::SimpleMenuModel* model = controller.menu_model();

  // Verify that the contextual secondary flyout container remains completely
  // omitted.
  EXPECT_FALSE(controller.shared_tabs_menu_model());

  // Traverse the primary flat menu structure to check if the candidate tab is
  // listed.
  bool found_tab1 = false;
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    std::u16string label = model->GetLabelAt(i);
    if (label.find(u"title1") != std::u16string::npos) {
      found_tab1 = true;
      break;
    }
  }

  EXPECT_TRUE(found_tab1);
}

// Verifies that when the flags are disabled, the checkmark is rendered on the
// left-hand side  by replacing the default model icon, instead of the
// right-hand side.
IN_PROC_BROWSER_TEST_F(
    OmniboxContextMenuControllerPecBrowserTestWithFlagsDisabled,
    VerifyModelPickerCheckmark_FlagOff) {
  // Navigate to the target WebUI URL where the contextual handler is hosted.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));
  auto* web_contents = GetWebContents();

  // Build the expected checkmark representation for matching menu assets later.
  auto check_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
      ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);

  // Force the popup state manager into AIM mode to mimic active UI conditions.
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Initialize and inject an InputState mock configured with active model
  // selections.
  omnibox::InputState input_state;
  input_state.allowed_models.push_back(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  input_state.allowed_models.push_back(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI);
  input_state.active_model =
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;

  auto* web_ui = web_contents->GetWebUI();
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);
  ASSERT_TRUE(handler->input_state_model());
  handler->input_state_model()->set_state_for_testing(input_state);

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  // Instantiate the controller which triggers menu hierarchy construction.
  OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                          web_contents);

  ui::SimpleMenuModel* model = controller.menu_model();

  // Map out the bound backing command identifiers created for the target model.
  int fast_command_id = -1;
  for (const auto& pair : controller.model_for_command_id_) {
    if (pair.second == omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI) {
      fast_command_id = pair.first;
      break;
    }
  }
  ASSERT_NE(fast_command_id, -1);
  {
    // Validate that the checkmark falls back to the left primary slot rather
    // than the right secondary slot.
    std::optional<size_t> index = model->GetIndexOfCommandId(fast_command_id);
    ASSERT_TRUE(index.has_value());
    EXPECT_TRUE(model->GetMinorIconAt(index.value()).IsEmpty());
    EXPECT_FALSE(model->GetIconAt(index.value()).IsEmpty());
    EXPECT_EQ(model->GetIconAt(index.value()), check_icon);
  }
}

class OmniboxContextMenuControllerContextManagementBrowserTest
    : public OmniboxContextMenuControllerBrowserTest {
 public:
  OmniboxContextMenuControllerContextManagementBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kContextManagementInComposebox);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerContextManagementBrowserTest,
                       SharedTabsSubmenuDynamicLabel) {
  // Navigate the initial tab to the popup URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));
  auto* web_contents = GetWebContents();

  // Set the popup state to composebox (AIM) so that session handle and
  // composebox handler are active.
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Set up an override to construct `MockTabContextualizationController`
  // for all tabs in this test.
  ui::UserDataFactory::ScopedOverride controller_override =
      CreateMockTabContextualizationControllerOverride();

  // Add two tabs.
  tabs::TabInterface* tab1 =
      AddTabToBrowser(1, embedded_test_server()->GetURL("/title2.html"));
  tabs::TabInterface* tab2 =
      AddTabToBrowser(2, embedded_test_server()->GetURL("/title3.html"));
  ASSERT_TRUE(tab1 && tab2);

  int32_t tab1_id = tab1->GetHandle().raw_value();
  int32_t tab2_id = tab2->GetHandle().raw_value();

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  auto* web_ui = web_contents->GetWebUI();
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);

  // Helper lambda to get the submenu parent item's label.
  auto get_submenu_label = [&]() {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            web_contents);
    std::optional<size_t> index = controller.menu_model()->GetIndexOfCommandId(
        IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU);
    if (index.has_value()) {
      return controller.menu_model()->GetLabelAt(index.value());
    }
    return std::u16string();
  };

  // 0 tabs selected/checked. Title should be "Add tabs".
  EXPECT_EQ(get_submenu_label(),
            l10n_util::GetStringUTF16(IDS_COMPOSE_ADD_TABS));

  // 1 tab selected/checked. Title should be "Sharing 1 tab".
  handler->AddTabContext(tab1_id, /*delay_upload=*/false,
                         searchbox::mojom::TabAttachmentSource::kContextMenu,
                         base::DoNothing());
  EXPECT_EQ(get_submenu_label(), u"Sharing 1 tab");

  // 2 tabs selected/checked. Title should be "Sharing 2 tabs".
  handler->AddTabContext(tab2_id, /*delay_upload=*/false,
                         searchbox::mojom::TabAttachmentSource::kContextMenu,
                         base::DoNothing());
  EXPECT_EQ(get_submenu_label(), u"Sharing 2 tabs");
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       TabsSubmenuDisabledWhenTabContextDisabled) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));

  auto* web_contents = GetWebContents();
  auto owning_window = gfx::NativeWindow();
  TestOmniboxPopupFileSelector file_selector(owning_window);

  auto* web_ui = web_contents->GetWebUI();
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  auto* handler = popup_ui->composebox_handler();

  // Set input state where BROWSER_TAB is allowed but disabled.
  omnibox::InputState test_state;
  test_state.allowed_input_types.emplace_back(
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  test_state.disabled_input_types.emplace_back(
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  handler->input_state_model()->set_state_for_testing(test_state);

  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  OmniboxContextMenuController controller(&file_selector, web_contents);

  // The submenu command ID should be disabled.
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU));

  // The submenu label and icon should be styled as disabled.
  auto* menu_model = controller.menu_model();
  std::optional<size_t> submenu_index =
      menu_model->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU);
  ASSERT_TRUE(submenu_index.has_value());
  EXPECT_EQ(menu_model->GetForegroundColorId(submenu_index.value()),
            ui::kColorMenuItemForegroundDisabled);
  EXPECT_EQ(menu_model->GetSelectedBackgroundColorId(submenu_index.value()),
            ui::kColorMenuItemBackgroundSelected);
  EXPECT_EQ(
      menu_model->GetIconAt(submenu_index.value()),
      ui::ImageModel::FromVectorIcon(kTabOldIcon, ui::kColorMenuIconDisabled,
                                     ui::SimpleMenuModel::kDefaultIconSize));
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       VerifyTabEnablementWhenLimitReached_NonPec) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));
  auto* popup_web_contents = GetWebContents();

  // Add two additional tabs.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndex(2, url2, ui::PAGE_TRANSITION_TYPED));

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  auto* web_ui = popup_web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  ASSERT_TRUE(popup_ui);
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);

  // Set max limit to 0 (so any unchecked tab is disabled).
  omnibox::InputState test_state;
  test_state.allowed_models.emplace_back(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  test_state.max_total_inputs = 0;
  handler->input_state_model()->set_state_for_testing(test_state);

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(popup_web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Get the tab IDs.
  auto* tab_strip_model = browser()->tab_strip_model();
  auto* tab1 = tab_strip_model->GetTabAtIndex(1);
  int32_t tab1_id = tab1->GetHandle().raw_value();

  // Manually add tab 1 to composebox_handler->selected_tabs to mark it checked.
  auto token1 = base::UnguessableToken::Create();
  handler->selected_tabs[token1] = tab1_id;

  // Construct controller.
  OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                          popup_web_contents);

  // Tab 1 (checked) sorted first (33000) -> should be enabled.
  // Tab 2 (unchecked) sorted second (33001) -> should be disabled.
  EXPECT_TRUE(controller.IsCommandIdEnabled(33000));
  EXPECT_FALSE(controller.IsCommandIdEnabled(33001));
}

// TODO(crbug.com/530351886): Times out flakily on Linux, Win, Mac and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
#define MAYBE_VerifyTabEnablementWhenMaxInputsReached \
  DISABLED_VerifyTabEnablementWhenMaxInputsReached
#else
#define MAYBE_VerifyTabEnablementWhenMaxInputsReached \
  VerifyTabEnablementWhenMaxInputsReached
#endif
IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerPecBrowserTest,
                       MAYBE_VerifyTabEnablementWhenMaxInputsReached) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupAimURL)));
  auto* popup_web_contents = GetWebContents();

  // Add two additional tabs.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndex(2, url2, ui::PAGE_TRANSITION_TYPED));

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  auto* web_ui = popup_web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  auto* popup_ui = web_ui->GetController()->GetAs<OmniboxPopupUI>();
  ASSERT_TRUE(popup_ui);
  auto* handler = popup_ui->composebox_handler();
  ASSERT_TRUE(handler);

  // Set input state where BROWSER_TAB is allowed but disabled (limit reached).
  omnibox::InputState test_state;
  test_state.allowed_input_types.emplace_back(
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  test_state.disabled_input_types.emplace_back(
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  handler->input_state_model()->set_state_for_testing(test_state);

  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::FromWebContents(popup_web_contents)
          ->get_omnibox_controller();
  ASSERT_TRUE(omnibox_controller);
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  // Initially, both tabs are unchecked. Since tab context is disabled, both
  // should be disabled.
  {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            popup_web_contents);
    EXPECT_FALSE(controller.IsCommandIdEnabled(33000));
    EXPECT_FALSE(controller.IsCommandIdEnabled(33001));
  }

  // Step 1: Start with tab context enabled.
  test_state.disabled_input_types.clear();
  handler->input_state_model()->set_state_for_testing(test_state);

  // Step 2: Select tab 1.
  {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            popup_web_contents);
    controller.ExecuteCommand(33000, 0);
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return !handler->selected_tabs.empty(); }));
  }

  // Step 3: Disable tab context.
  test_state.disabled_input_types.push_back(
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  handler->input_state_model()->set_state_for_testing(test_state);

  // Step 4: Verify.
  {
    OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                            popup_web_contents);
    // Tab 1 (checked) sorted first (33000) -> should be enabled.
    // Tab 2 (unchecked) sorted second (33001) -> should be disabled.
    EXPECT_TRUE(controller.IsCommandIdEnabled(33000));
    EXPECT_FALSE(controller.IsCommandIdEnabled(33001));
  }
}

// Recent tab/Current tab should not show since context management flag is
// disabled by default.
IN_PROC_BROWSER_TEST_F(OmniboxInlineTabsContextMenuBrowserTest,
                       InlineTabsDoNotRenderRecentOrCurrentTabLabel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupURL)));
  auto* web_contents = GetWebContents();

  // Add two tabs: active tab and a background (recent) tab.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndex(2, url2, ui::PAGE_TRANSITION_TYPED));

  browser()->tab_strip_model()->ActivateTabAt(0);

  auto owning_window = gfx::NativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  OmniboxContextMenuController controller(omnibox_popup_file_selector.get(),
                                          web_contents);
  ui::SimpleMenuModel* model = controller.menu_model();

  // Without the flag enabled (kContextManagementInOmnibox disabled), no shared
  // tabs submenu is created.
  EXPECT_FALSE(controller.shared_tabs_menu_model());

  // Verify that neither 'Recent tab' nor 'Current tab' label is rendered
  // on any item in the menu model.
  std::u16string recent_tab_label =
      l10n_util::GetStringUTF16(IDS_NTP_COMPOSEBOX_RECENT_TAB_SUFFIX);
  std::u16string current_tab_label =
      l10n_util::GetStringUTF16(IDS_COMPOSE_CURRENT_TAB);

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    EXPECT_NE(model->GetMinorTextAt(i), recent_tab_label);
    EXPECT_NE(model->GetMinorTextAt(i), current_tab_label);
    EXPECT_EQ(model->GetMinorTextAt(i), std::u16string());
  }
}

// Context menu omnibox flag is on.
IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       VerifySubmenuContextMenuMaxWidth) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupURL)));
  auto* web_contents = GetWebContents();

  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  OmniboxContextMenu context_menu(nullptr, omnibox_popup_file_selector.get(),
                                  web_contents);

  // Direct `GetMinimumMenuWidth` test for main menu ("menu == menu_").
  EXPECT_EQ(context_menu.GetMinimumMenuWidth(context_menu.menu()), 240);
  EXPECT_EQ(context_menu.GetMaxWidthForMenu(context_menu.menu()), 240);

  // Direct `GetMinimumMenuWidth` test for submenu item (where "menu != menu_").
  views::MenuItemView* submenu_item = context_menu.menu()->GetMenuItemByID(
      IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU);
  ASSERT_TRUE(submenu_item);
  EXPECT_EQ(context_menu.GetMinimumMenuWidth(submenu_item), 320);
  EXPECT_EQ(context_menu.GetMaxWidthForMenu(submenu_item), 320);

  // Verify `set_minimum_preferred_width` behavior on `SubmenuView`.
  context_menu.menu()->GetSubmenu()->set_minimum_preferred_width(
      context_menu.GetMinimumMenuWidth(context_menu.menu()));
  EXPECT_GE(context_menu.menu()->GetSubmenu()->GetPreferredSize({}).width(),
            240);

  submenu_item->GetSubmenu()->set_minimum_preferred_width(
      context_menu.GetMinimumMenuWidth(submenu_item));
  EXPECT_GE(submenu_item->GetSubmenu()->GetPreferredSize({}).width(), 320);
}

// Context menu omnibox flag is off.
IN_PROC_BROWSER_TEST_F(OmniboxInlineTabsContextMenuBrowserTest,
                       InlineTabsUseDefaultMenuWidth) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOmniboxPopupURL)));
  auto* web_contents = GetWebContents();

  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));

  auto owning_window = browser()->GetWindow()->GetNativeWindow();
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>(owning_window);

  OmniboxContextMenu context_menu(nullptr, omnibox_popup_file_selector.get(),
                                  web_contents);

  // When tabs are inline (no submenu), main menu width is 320px.
  EXPECT_EQ(context_menu.GetMinimumMenuWidth(context_menu.menu()), 320);
  EXPECT_EQ(context_menu.GetMaxWidthForMenu(context_menu.menu()), 320);
}
