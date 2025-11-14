// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/menus/simple_menu_model.h"

// Override `OpenAiMode` to track calls.
class TestOmniboxEditModelForContextMenu : public OmniboxEditModel {
 public:
  TestOmniboxEditModelForContextMenu(OmniboxController* controller,
                                     PrefService* pref_service)
      : OmniboxEditModel(controller) {}
  ~TestOmniboxEditModelForContextMenu() override = default;

  void OpenAiMode(bool via_keyboard, bool via_context_menu) override {
    ai_mode_open_calls_++;
  }

  int ai_mode_open_calls() const { return ai_mode_open_calls_; }

 private:
  int ai_mode_open_calls_ = 0;
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
      OmniboxEditModel* edit_model) override {
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
  OmniboxContextMenuControllerBrowserTest() = default;

  OmniboxContextMenuControllerBrowserTest(
      const OmniboxContextMenuControllerBrowserTest&) = delete;
  OmniboxContextMenuControllerBrowserTest& operator=(
      const OmniboxContextMenuControllerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       AddRecentTabsToMenu) {
  // TODO(crbug.com/458463536): Use proper web contents for the
  // omnibox_webui_popup.
  auto omnibox_popup_file_selector =
      std::make_unique<OmniboxPopupFileSelector>();
  OmniboxContextMenuController base_controller(
      browser()->tab_strip_model()->GetActiveWebContents(),
      omnibox_popup_file_selector.get(), nullptr, nullptr);
  ui::SimpleMenuModel* model = base_controller.menu_model();

  // The 1 separator and 4 static items.
  EXPECT_EQ(5u, model->GetItemCount());

  // Navigate the initial tab and add a new one to have exactly two tabs.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_TYPED));

  OmniboxContextMenuController controller(
      browser()->tab_strip_model()->GetActiveWebContents(),
      omnibox_popup_file_selector.get(), nullptr, nullptr);
  model = controller.menu_model();

  // The model should have 9 items, one for each tab,
  // and 1 header, 2 separators and 4 static items.
  EXPECT_EQ(9u, model->GetItemCount());
}

IN_PROC_BROWSER_TEST_F(OmniboxContextMenuControllerBrowserTest,
                       ExecuteCommand) {
  TestingPrefServiceSimple pref_service;
  TestOmniboxEditModelForContextMenu edit_model(nullptr, &pref_service);
  TestOmniboxPopupFileSelector file_selector;
  OmniboxContextMenuController controller(
      browser()->tab_strip_model()->GetActiveWebContents(), &file_selector,
      nullptr, &edit_model);

  BrowserWindowInterface* browser_window_interface =
      webui::GetBrowserWindowInterface(
          browser()->tab_strip_model()->GetActiveWebContents());
  SearchboxContextData* searchbox_context_data =
      browser_window_interface->GetFeatures().searchbox_context_data();
  ASSERT_TRUE(searchbox_context_data);

  // Test Add Image.
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_ADD_IMAGE, 0);
  EXPECT_EQ(1, file_selector.open_file_upload_dialog_calls());

  // Test Add File.
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_ADD_FILE, 0);
  EXPECT_EQ(2, file_selector.open_file_upload_dialog_calls());

  // Test Deep Search.
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH, 0);
  EXPECT_EQ(1, edit_model.ai_mode_open_calls());
  auto context = searchbox_context_data->TakePendingContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(context->mode, searchbox::mojom::ToolMode::kDeepSearch);
  searchbox_context_data->SetPendingContext(std::move(context));

  // Test Create Image.
  controller.ExecuteCommand(IDC_OMNIBOX_CONTEXT_CREATE_IMAGES, 0);
  EXPECT_EQ(2, edit_model.ai_mode_open_calls());
  context = searchbox_context_data->TakePendingContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(context->mode, searchbox::mojom::ToolMode::kCreateImage);
}
