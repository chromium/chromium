// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace autofill {

class AutofillPopupControllerBrowserTest : public InProcessBrowserTest,
                                           public content::WebContentsObserver {
 public:
  AutofillPopupControllerBrowserTest() {}
  ~AutofillPopupControllerBrowserTest() override {}

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents != NULL);
    Observe(web_contents);

    ContentAutofillDriver* driver =
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(web_contents->GetMainFrame());
    autofill_external_delegate_ =
        std::make_unique<TestAutofillExternalDelegate>(
            driver->autofill_manager(), driver,
            /*call_parent_methods=*/true);

    disable_animation_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  // Normally the WebContents will automatically delete the delegate, but here
  // the delegate is owned by this test, so we have to manually destroy.
  void RenderFrameDeleted(content::RenderFrameHost* rfh) override {
    if (!rfh->GetParent())
      autofill_external_delegate_.reset();
  }

 protected:
  std::unique_ptr<TestAutofillExternalDelegate> autofill_external_delegate_;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> disable_animation_;
};

#if defined(OS_MAC)
// Fails on Mac OS. http://crbug.com/453256
#define MAYBE_HidePopupOnWindowMove DISABLED_HidePopupOnWindowMove
#else
#define MAYBE_HidePopupOnWindowMove HidePopupOnWindowMove
#endif
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       MAYBE_HidePopupOnWindowMove) {
  test::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  EXPECT_FALSE(autofill_external_delegate_->popup_hidden());

  // Move the window, which should close the popup.
  gfx::Rect new_bounds = browser()->window()->GetBounds() - gfx::Vector2d(1, 1);
  browser()->window()->SetBounds(new_bounds);

  autofill_external_delegate_->WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate_->popup_hidden());
}

IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       HidePopupOnWindowResize) {
  test::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  EXPECT_FALSE(autofill_external_delegate_->popup_hidden());

  // Resize the window, which should cause the popup to hide.
  gfx::Rect new_bounds = browser()->window()->GetBounds();
  new_bounds.Inset(1, 1);
  browser()->window()->SetBounds(new_bounds);

  autofill_external_delegate_->WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate_->popup_hidden());
}

// This test checks that the browser doesn't crash if the delegate is deleted
// before the popup is hidden.
#if defined(OS_MAC)
// Flaky on Mac 10.9 in debug mode. http://crbug.com/710439
#define MAYBE_DeleteDelegateBeforePopupHidden \
  DISABLED_DeleteDelegateBeforePopupHidden
#else
#define MAYBE_DeleteDelegateBeforePopupHidden DeleteDelegateBeforePopupHidden
#endif
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       MAYBE_DeleteDelegateBeforePopupHidden) {
  test::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  // Delete the external delegate here so that is gets deleted before popup is
  // hidden. This can happen if the web_contents are destroyed before the popup
  // is hidden. See http://crbug.com/232475
  autofill_external_delegate_.reset();
}

// crbug.com/965025
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest, ResetSelectedLine) {
  test::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  auto* client =
      autofill::ChromeAutofillClient::FromWebContents(web_contents());
  AutofillPopupController* controller =
      client->popup_controller_for_testing().get();
  ASSERT_TRUE(controller);

  // Push some suggestions and select the line #3.
  std::vector<std::u16string> rows = {u"suggestion1", u"suggestion2",
                                      u"suggestion3", u"suggestion4"};
  client->UpdateAutofillPopupDataListValues(rows, rows);
  int original_suggestions_count = controller->GetLineCount();
  controller->SetSelectedLine(3);

  // Replace the list with the smaller one.
  rows = {u"suggestion1"};
  client->UpdateAutofillPopupDataListValues(rows, rows);
  // Make sure that previously selected line #3 doesn't exist.
  ASSERT_LT(controller->GetLineCount(), original_suggestions_count);
  // Selecting a new line should not crash.
  controller->SetSelectedLine(0);
}

}  // namespace autofill
