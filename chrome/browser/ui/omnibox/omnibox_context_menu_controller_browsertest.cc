// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/menus/simple_menu_model.h"

// Override `OpenFileUploadDialog` to track calls.
class TestOmniboxPopupFileSelector : public OmniboxPopupFileSelector {
 public:
  explicit TestOmniboxPopupFileSelector(gfx::NativeWindow owning_window)
      : OmniboxPopupFileSelector(owning_window) {}
  ~TestOmniboxPopupFileSelector() override = default;

  void OpenFileUploadDialog(
      content::WebContents* web_contents,
      bool is_image,
      OmniboxEditModel* edit_model,
      std::optional<lens::ImageEncodingOptions> image_encoding_options,
      bool was_ai_mode_open) override {
    open_file_upload_dialog_calls_++;
    last_was_ai_mode_open_ = was_ai_mode_open;
    edit_model_ = edit_model;
  }

  void FileSelectionCanceled() override {
    if (last_was_ai_mode_open_ && edit_model_) {
      edit_model_->OpenAiMode(false, true);
    }
  }

  int open_file_upload_dialog_calls() const {
    return open_file_upload_dialog_calls_;
  }

  bool last_was_ai_mode_open() const { return last_was_ai_mode_open_; }

 private:
  int open_file_upload_dialog_calls_ = 0;
  bool last_was_ai_mode_open_ = false;
  raw_ptr<OmniboxEditModel> edit_model_ = nullptr;
};

class OmniboxContextMenuControllerBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxContextMenuControllerBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{omnibox::internal::kWebUIOmniboxAimPopup,
          {{omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
            "inline"},
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
  auto* web_contents = GetWebContents();
  // TODO(crbug.com/458463536): Use proper web contents for the
  // aim popup.
  auto owning_window = gfx::NativeWindow();
  auto omnibox_popup_file_selector = std::make_unique<OmniboxPopupFileSelector>(
      owning_window);
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
