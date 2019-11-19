// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_view.h"

#include "base/strings/string16.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view_tester.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"

namespace autofill {

class TestPasswordGenerationPopupController
    : public PasswordGenerationPopupControllerImpl {
 public:
  explicit TestPasswordGenerationPopupController(
      content::WebContents* web_contents)
      : PasswordGenerationPopupControllerImpl(
            gfx::RectF(0, 0, 10, 10),
            autofill::password_generation::PasswordGenerationUIData(
                /*bounds=*/gfx::RectF(0, 0, 10, 10),
                /*max_length=*/10,
                /*generation_element=*/base::string16(),
                /*generation_element_renderer_id=*/100,
                /*is_generation_element_password_type=*/true,
                /*text_direction=*/base::i18n::TextDirection(),
                PasswordForm()),
            password_manager::ContentPasswordManagerDriverFactory::
                FromWebContents(web_contents)
                    ->GetDriverForFrame(web_contents->GetMainFrame())
                    ->AsWeakPtr(),
            nullptr /* PasswordGenerationPopupObserver*/,
            web_contents,
            web_contents->GetMainFrame()) {}

  ~TestPasswordGenerationPopupController() override {}

  PasswordGenerationPopupView* view() { return view_; }
};

class PasswordGenerationPopupViewTest : public InProcessBrowserTest {
 public:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::unique_ptr<PasswordGenerationPopupViewTester> GetViewTester() {
    return PasswordGenerationPopupViewTester::For(controller_->view());
  }

 protected:
  TestPasswordGenerationPopupController* controller_;
};

// Regression test for crbug.com/400543. Verifying that moving the mouse in the
// editing dialog doesn't crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       MouseMovementInEditingPopup) {
  controller_ =
      new autofill::TestPasswordGenerationPopupController(GetWebContents());
  controller_->Show(PasswordGenerationPopupController::kEditGeneratedPassword);

  GetViewTester()->SimulateMouseMovementAt(gfx::Point(1, 1));

  // This hides the popup and destroys the controller.
  GetWebContents()->Close();
}

// Verify that destroying web contents with visible popup does not crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       CloseWebContentsWithVisiblePopup) {
  controller_ =
      new autofill::TestPasswordGenerationPopupController(GetWebContents());
  controller_->Show(PasswordGenerationPopupController::kEditGeneratedPassword);

  GetWebContents()->Close();
}

}  // namespace autofill
