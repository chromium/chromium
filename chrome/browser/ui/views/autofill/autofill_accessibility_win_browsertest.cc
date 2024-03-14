// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_variant.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace autofill {

class AutofillAccessibilityWinBrowserTest : public InProcessBrowserTest {
 public:
  AutofillAccessibilityWinBrowserTest() = default;

  AutofillAccessibilityWinBrowserTest(
      const AutofillAccessibilityWinBrowserTest&) = delete;
  AutofillAccessibilityWinBrowserTest& operator=(
      const AutofillAccessibilityWinBrowserTest&) = delete;

 protected:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver, "en-US") {}

    testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {AutofillManagerEvent::kFormsSeen}};
  };

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    scoped_accessibility_mode_.emplace(GetWebContents(), ui::kAXModeComplete);
  }

  void TearDownOnMainThread() override { scoped_accessibility_mode_.reset(); }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  HWND GetWebPageHwnd() const {
    return browser()
        ->window()
        ->GetNativeWindow()
        ->GetHost()
        ->GetAcceleratedWidget();
  }

  TestAutofillManager* GetAutofillManager() {
    return autofill_manager_injector_[GetWebContents()];
  }

  void NavigateToAndWaitForForm(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(GetAutofillManager()->WaitForFormsSeen(1));
  }

  // Show drop down based on the element id.
  void ShowDropdown(const std::string& field_id) {
    std::string js("document.getElementById('" + field_id + "').focus();");
    ASSERT_TRUE(ExecJs(GetWebContents(), js));
    SendKeyToPage(GetWebContents(), ui::DomKey::ARROW_DOWN);
  }

  void SendKeyToPage(content::WebContents* web_contents, const ui::DomKey key) {
    ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
    ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
    SimulateKeyPress(web_contents, key, code, key_code, false, false, false,
                     false);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{::features::kUiaProvider};
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
  std::optional<content::ScopedAccessibilityModeOverride>
      scoped_accessibility_mode_;
};

// The test is flaky on Windows. See https://crbug.com/1221273
#if BUILDFLAG(IS_WIN)
#define MAYBE_AutofillPopupControllerFor DISABLED_AutofillPopupControllerFor
#else
#define MAYBE_AutofillPopupControllerFor AutofillPopupControllerFor
#endif
IN_PROC_BROWSER_TEST_F(AutofillAccessibilityWinBrowserTest,
                       MAYBE_AutofillPopupControllerFor) {
  content::AccessibilityNotificationWaiter waiter(
      GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
  NavigateToAndWaitForForm(
      embedded_test_server()->GetURL("/accessibility/input_datalist.html"));
  ASSERT_TRUE(waiter.WaitForNotification());

  base::win::ScopedVariant result_variant;

  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kTextFieldWithComboBox;

  // The autofill popup of the form input element has not shown yet. The form
  // input element is the controller for the checkbox as indicated by the form
  // input element's |aria-controls| attribute.
  content::UiaGetPropertyValueVtArrayVtUnknownValidate(
      UIA_ControllerForPropertyId,
      FindAccessibilityNode(GetWebContents(), find_criteria), {"checkbox"});

  UiaAccessibilityWaiterInfo info = {
      GetWebPageHwnd(), base::ASCIIToWide("combobox"),
      base::ASCIIToWide("input"), ax::mojom::Event::kControlsChanged};

  std::unique_ptr<UiaAccessibilityEventWaiter> control_waiter =
      std::make_unique<UiaAccessibilityEventWaiter>(info);
  // Show popup and wait for UIA_ControllerForPropertyId event.
  ShowDropdown("datalist");
  control_waiter->Wait();

  // The focus should remain on the input element.
  EXPECT_EQ(content::GetFocusedAccessibilityNodeInfo(GetWebContents()).role,
            ax::mojom::Role::kTextFieldWithComboBox);

  // The autofill popup of the form input element is showing. The form input
  // element is the controller for the checkbox and autofill popup as
  // indicated by the form input element's |aria-controls| attribute and the
  // existing popup.
  content::UiaGetPropertyValueVtArrayVtUnknownValidate(
      UIA_ControllerForPropertyId,
      FindAccessibilityNode(GetWebContents(), find_criteria),
      {"checkbox", "Autofill"});

  control_waiter = std::make_unique<UiaAccessibilityEventWaiter>(info);
  // Hide popup and wait for UIA_ControllerForPropertyId event.
  SendKeyToPage(GetWebContents(), ui::DomKey::TAB);
  control_waiter->Wait();

  // The autofill popup of the form input element is hidden. The form
  // input element is the controller for the checkbox as indicated by the form
  // input element's |aria-controls| attribute.
  content::UiaGetPropertyValueVtArrayVtUnknownValidate(
      UIA_ControllerForPropertyId,
      FindAccessibilityNode(GetWebContents(), find_criteria), {"checkbox"});
}

}  // namespace autofill
