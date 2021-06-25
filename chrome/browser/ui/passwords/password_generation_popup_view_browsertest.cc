// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_view.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view_tester.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/prerender_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "url/gurl.h"

namespace autofill {

class TestPasswordGenerationPopupController
    : public PasswordGenerationPopupControllerImpl {
 public:
  // |vertical_offset| specifies the vertical offset of the popup relative to
  // the web contents container.
  explicit TestPasswordGenerationPopupController(
      content::WebContents* web_contents,
      int vertical_offset = 0)
      : PasswordGenerationPopupControllerImpl(
            gfx::RectF(web_contents->GetContainerBounds().x(),
                       web_contents->GetContainerBounds().y() + vertical_offset,
                       10,
                       10),
            autofill::password_generation::PasswordGenerationUIData(
                /*bounds=*/gfx::RectF(web_contents->GetContainerBounds().x(),
                                      web_contents->GetContainerBounds().y(),
                                      10,
                                      10),
                /*max_length=*/10,
                /*generation_element=*/std::u16string(),
                autofill::FieldRendererId(100),
                /*is_generation_element_password_type=*/true,
                /*text_direction=*/base::i18n::TextDirection(),
                FormData()),
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
  EXPECT_TRUE(controller_->Show(
      PasswordGenerationPopupController::kEditGeneratedPassword));

  GetViewTester()->SimulateMouseMovementAt(
      gfx::Point(GetWebContents()->GetContainerBounds().x() + 1,
                 GetWebContents()->GetContainerBounds().y() + 1));

  // This hides the popup and destroys the controller.
  GetWebContents()->Close();
}

// Verify that destroying web contents with visible popup does not crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       CloseWebContentsWithVisiblePopup) {
  controller_ =
      new autofill::TestPasswordGenerationPopupController(GetWebContents());
  EXPECT_TRUE(controller_->Show(
      PasswordGenerationPopupController::kEditGeneratedPassword));

  GetWebContents()->Close();
}

// Verify that controller is not crashed in case of insufficient vertical space
// for showing popup.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       DoNotCrashInCaseOfInsuffucientVertialSpace) {
  controller_ = new autofill::TestPasswordGenerationPopupController(
      GetWebContents(), /*vertical_offset=*/-20);
  EXPECT_FALSE(controller_->Show(
      PasswordGenerationPopupController::kEditGeneratedPassword));
}

class PasswordGenerationPopupViewPrerenderingTest
    : public PasswordGenerationPopupViewTest {
 public:
  PasswordGenerationPopupViewPrerenderingTest()
      : prerender_helper_(base::BindRepeating(
            &PasswordGenerationPopupViewTest::GetWebContents,
            base::Unretained(this))) {}
  ~PasswordGenerationPopupViewPrerenderingTest() override = default;

  void SetUpOnMainThread() override {
    prerender_helper_.SetUpOnMainThread(embedded_test_server());
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewPrerenderingTest,
                       PasswordGenerationPopupControllerInPrerendering) {
  GURL url =
      embedded_test_server()->GetURL("/password/nonplaceholder_username.html");
  ui_test_utils::NavigateToURL(browser(), url);

  autofill::password_generation::PasswordGenerationUIData ui_data(
      /*bounds=*/gfx::RectF(GetWebContents()->GetContainerBounds().x(),
                            GetWebContents()->GetContainerBounds().y(), 10, 10),
      /*max_length=*/20, u"element", autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData());
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, nullptr, nullptr, GetWebContents(),
          GetWebContents()->GetMainFrame());

  EXPECT_TRUE(controller->Show(
      PasswordGenerationPopupController::kEditGeneratedPassword));

  auto prerender_url =
      embedded_test_server()->GetURL("/password/password_form.html");
  // Loads a page in the prerender.
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  // It should keep the current popup controller since the prerenedering should
  // not affect the current page.
  EXPECT_NE(controller, nullptr);

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  // It should clear the current popup controller since the page loading deletes
  // the popup controller from the previous page.
  EXPECT_EQ(controller, nullptr);
}

}  // namespace autofill
