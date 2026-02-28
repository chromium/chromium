// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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
         {omnibox::kWebUIOmniboxPopup, {}}},
        /*disabled_features=*/{omnibox::kAimServerEligibilityEnabled,
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

  // The model should have the following visible items:
  //   - 1 header ("Most recent tabs")
  //   - 2 recent tab items
  //   - 1 separator
  //   - 2 contextual input items
  //   - 1 separator
  //   - 2 tool input items
  EXPECT_EQ(9u, GetVisibleItemCount(model));
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

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       RecordHistogramOnTabSelected) {
  base::HistogramTester histogram_tester;

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
  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kClassic);

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
