// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
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
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/menus/simple_menu_model.h"

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
         {omnibox::kContextManagementInOmnibox, {}}},
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
    LocationBar* location_bar = browser()->window()->GetLocationBar();
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
  auto owning_window = browser()->window()->GetNativeWindow();
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
  OpenClassicPopup(browser()->profile(), omnibox_controller);

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
    LocationBar* location_bar = browser()->window()->GetLocationBar();
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
  OpenClassicPopup(browser()->profile(), omnibox_controller);

  ASSERT_NE(command_id, -1) << "Drive option not found in menu";
  controller.ExecuteCommand(command_id, 0);

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
      features::IsRoundedIconsEnabled() ? kCheckIcon : kCheckOldIcon,
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

  auto owning_window = browser()->window()->GetNativeWindow();
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

// Explicitly enable both flags to activate right-hand side checkmark
// behavior.
class OmniboxContextMenuControllerBrowserTestWithContextManagement
    : public OmniboxContextMenuControllerBrowserTest {
 public:
  OmniboxContextMenuControllerBrowserTestWithContextManagement() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{omnibox::kContextManagementInComposebox,
                              omnibox::kContextManagementInOmnibox},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OmniboxContextMenuControllerBrowserTestWithContextManagement,
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
      tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
          base::BindRepeating(
              [](GURL url, tabs::TabInterface& tab)
                  -> std::unique_ptr<lens::TabContextualizationController> {
                auto mock =
                    std::make_unique<MockTabContextualizationController>(&tab);
                EXPECT_CALL(*mock, GetPageContext)
                    .WillOnce([url](lens::TabContextualizationController::
                                        GetPageContextCallback callback) {
                      auto data = std::make_unique<lens::ContextualInputData>();
                      data->is_page_context_eligible = true;
                      data->page_url = url;
                      data->page_title = "Title 1";
                      data->primary_content_type =
                          lens::MimeType::kAnnotatedPageContent;
                      std::move(callback).Run(std::move(data));
                    });
                return mock;
              },
              url));

  // Add a recent tab (created with the mock controller) in the background.
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, url,
                                     ui::PAGE_TRANSITION_TYPED,
                                     /*check_navigation_success=*/false));

  auto owning_window = browser()->window()->GetNativeWindow();
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
    handler->AddTabContext(tab_id, /*delay_upload=*/false, base::DoNothing());

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
      tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
          base::BindRepeating(
              [](tabs::TabInterface& tab)
                  -> std::unique_ptr<lens::TabContextualizationController> {
                auto mock =
                    std::make_unique<MockTabContextualizationController>(&tab);
                EXPECT_CALL(*mock, GetPageContext)
                    .WillRepeatedly([&tab](
                                        lens::TabContextualizationController::
                                            GetPageContextCallback callback) {
                      auto data = std::make_unique<lens::ContextualInputData>();
                      data->is_page_context_eligible = true;
                      data->page_url = tab.GetContents()->GetLastCommittedURL();
                      data->page_title = "Title";
                      data->primary_content_type =
                          lens::MimeType::kAnnotatedPageContent;
                      std::move(callback).Run(std::move(data));
                    });
                return mock;
              }));

  // Add three tabs.
  GURL url1(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, url1,
                                     ui::PAGE_TRANSITION_TYPED,
                                     /*check_navigation_success=*/false));

  GURL url2(embedded_test_server()->GetURL("/title3.html"));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 2, url2,
                                     ui::PAGE_TRANSITION_TYPED,
                                     /*check_navigation_success=*/false));

  GURL url3(embedded_test_server()->GetURL("/simple.html"));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 3, url3,
                                     ui::PAGE_TRANSITION_TYPED,
                                     /*check_navigation_success=*/false));

  // Order of activation/recency: 3, then 2, then 1 (making 1 active/most
  // recent).
  browser()->tab_strip_model()->ActivateTabAt(3);
  browser()->tab_strip_model()->ActivateTabAt(2);
  browser()->tab_strip_model()->ActivateTabAt(1);

  tabs::TabInterface* tab2 = browser()->tab_strip_model()->GetTabAtIndex(2);
  int32_t tab2_id = tab2->GetHandle().raw_value();
  tabs::TabInterface* tab3 = browser()->tab_strip_model()->GetTabAtIndex(3);
  int32_t tab3_id = tab3->GetHandle().raw_value();

  auto owning_window = browser()->window()->GetNativeWindow();
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
      if (command_id >= kMinOmniboxContextMenuRecentTabsCommandId &&
          command_id < kMinOmniboxContextMenuRecentTabsCommandId +
                           controller.GetMaxTabSuggestions()) {
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
  handler->AddTabContext(tab3_id, /*delay_upload=*/false, base::DoNothing());

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
  handler->AddTabContext(tab2_id, /*delay_upload=*/false, base::DoNothing());

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
