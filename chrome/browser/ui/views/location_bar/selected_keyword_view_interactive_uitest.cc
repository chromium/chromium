// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"

#include "base/test/run_until.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/views_test_utils.h"

namespace {

void InputKeys(BrowserWindowInterface* browser,
               const std::vector<ui::KeyboardCode>& keys) {
  for (auto key : keys) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser, key, false, false,
                                                false, false));
  }
}

class SelectedKeywordViewTest : public extensions::ExtensionBrowserTest {
 public:
  SelectedKeywordViewTest() = default;

  SelectedKeywordViewTest(const SelectedKeywordViewTest&) = delete;
  SelectedKeywordViewTest& operator=(const SelectedKeywordViewTest&) = delete;

  ~SelectedKeywordViewTest() override = default;
};

// Tests that an extension's short name is registered as the value of the
// extension's omnibox keyword. When the extension's omnibox keyword is
// activated, then the selected keyword label in the omnibox should be the
// extension's short name.
IN_PROC_BROWSER_TEST_F(SelectedKeywordViewTest,
                       TestSelectedKeywordViewIsExtensionShortname) {
  const extensions::Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("omnibox"), 1);
  ASSERT_NE(extension, nullptr);

  BrowserWindowInterface* current_browser = browser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(current_browser));

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(current_browser);

  content::WebContents* web_ui_contents = nullptr;
  if (features::IsWebUILocationBarEnabled()) {
    web_ui_contents = browser_view->toolbar_button_provider()
                          ->GetWebUIToolbarViewForTesting()
                          ->GetWebViewForTesting()
                          ->GetWebContents();
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(web_ui_contents,
                             "Boolean(document.querySelector('toolbar-app')"
                             "?.shadowRoot?.querySelector('location-bar')"
                             "?.shadowRoot?.querySelector('readonly-omnibox')"
                             "?.shadowRoot?.querySelector('#textInput')"
                             "?.shadowRoot?.querySelector('#input'))")
          .ExtractBool();
    }));
    chrome::FocusLocationBar(current_browser);
    ASSERT_TRUE(content::ExecJs(web_ui_contents,
                                "document.querySelector('toolbar-app')"
                                ".shadowRoot.querySelector('location-bar')"
                                ".shadowRoot.querySelector('readonly-omnibox')"
                                ".shadowRoot.querySelector('#textInput')"
                                ".shadowRoot.querySelector('#input').focus()"));
  } else {
    chrome::FocusLocationBar(current_browser);
  }

  ASSERT_TRUE(ui_test_utils::IsViewFocused(current_browser, VIEW_ID_OMNIBOX));

  // Activate the extension's omnibox keyword.
  InputKeys(current_browser, {ui::VKEY_K, ui::VKEY_E, ui::VKEY_Y});

  auto* omnibox_controller =
      browser_view->GetLocationBar()->GetOmniboxController();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return omnibox_controller->IsPopupOpen() &&
           omnibox_controller->edit_model()->is_keyword_hint();
  }));
  if (features::IsWebUILocationBarEnabled()) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(web_ui_contents,
                             "Boolean(document.querySelector('toolbar-app')"
                             "?.shadowRoot?.querySelector('location-bar')"
                             "?.classList.contains('popup-open'))")
          .ExtractBool();
    }));
  }

  InputKeys(current_browser, {ui::VKEY_TAB});

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return omnibox_controller->edit_model()->is_keyword_selected();
  }));

  if (features::IsWebUILocationBarEnabled()) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(web_ui_contents,
                             "Boolean(document.querySelector('toolbar-app')"
                             "?.shadowRoot?.querySelector('location-bar')"
                             "?.shadowRoot?.querySelector('selected-keyword'))")
          .ExtractBool();
    }));
    std::string short_name =
        content::EvalJs(web_ui_contents,
                        "document.querySelector('toolbar-app')"
                        ".shadowRoot.querySelector('location-bar')"
                        ".shadowRoot.querySelector('selected-keyword')"
                        ".selectedKeywordState.shortName")
            .ExtractString();
    EXPECT_EQ(extension->short_name(), short_name);
  } else {
    SelectedKeywordView* selected_keyword_view =
        browser_view->toolbar()->location_bar_view()->selected_keyword_view();
    ASSERT_NE(selected_keyword_view, nullptr);

    views::test::RunScheduledLayout(browser_view);

    // Verify that the label in the omnibox is the extension's shortname.
    EXPECT_EQ(extension->short_name(),
              base::UTF16ToUTF8(selected_keyword_view->label()->GetText()));
  }
}

}  // namespace
