// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace autofill {

class AutofillPopupControllerBrowserTest : public InProcessBrowserTest {
 public:
  AutofillPopupControllerBrowserTest() = default;
  ~AutofillPopupControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    web_contents()->Focus();

    // The test cases mock the entire forms by directly calling
    // ContentAutofillDriver functions. Nonetheless we set up an HTTP server and
    // open an empty page. Otherwise, the FormData::url would be about:blank and
    // FormStructure::ShouldBeParsed() would be false, so the form wouldn't be
    // even parsed by AutofillManager.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html;charset=utf-8");
          response->set_content("");
          return response;
        }));
    embedded_test_server()->StartAcceptingConnections();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/test.html")));

    test_api(autofill_manager())
        .SetExternalDelegate(std::make_unique<TestAutofillExternalDelegate>(
            &autofill_manager(),
            /*call_parent_methods=*/true));

    disable_animation_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ContentAutofillDriver& autofill_driver() {
    return *ContentAutofillDriver::GetForRenderFrameHost(main_rfh());
  }

  BrowserAutofillManager& autofill_manager() {
    return static_cast<BrowserAutofillManager&>(
        autofill_driver().GetAutofillManager());
  }

  Profile* profile() { return browser()->profile(); }

  TestAutofillExternalDelegate& autofill_external_delegate() {
    return static_cast<TestAutofillExternalDelegate&>(
        *test_api(autofill_manager()).external_delegate());
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> disable_animation_;
};

IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       HidePopupOnWindowMove) {
  EXPECT_TRUE(GenerateTestAutofillPopup(autofill_driver(), profile(),
                                        /*expect_popup_to_be_shown=*/true));

  // Move the window, which should close the popup.
  gfx::Rect new_bounds = browser()->window()->GetBounds() - gfx::Vector2d(1, 1);
  browser()->window()->SetBounds(new_bounds);

  autofill_external_delegate().WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate().popup_hidden());
}

IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       HidePopupOnWindowResize) {
  EXPECT_TRUE(GenerateTestAutofillPopup(autofill_driver(), profile(),
                                        /*expect_popup_to_be_shown=*/true));

  // Resize the window, which should cause the popup to hide.
  gfx::Rect new_bounds = browser()->window()->GetBounds();
  new_bounds.Inset(1);
  browser()->window()->SetBounds(new_bounds);

  autofill_external_delegate().WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate().popup_hidden());
}

IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       DoNotShowIfNotEnoughSpace) {
  constexpr float kSize = 100.0f;
  // Set to smallest possible size. The actual minimum size is larger and
  // platform dependent.
  browser()->window()->SetBounds(gfx::Rect(1, 1));
  gfx::Rect window_bounds = browser()->window()->GetBounds();
  // Position the popup in the lower right corner so that there is not enough
  // space to display it.
  EXPECT_TRUE(GenerateTestAutofillPopup(
      autofill_driver(), profile(),
      /*expect_popup_to_be_shown=*/false, /*element_bounds=*/
      gfx::RectF(window_bounds.x() - kSize, window_bounds.y() - kSize, kSize,
                 kSize)));
}

// Tests that entering fullscreen hides the popup and, in particular, does not
// crash (crbug.com/1267047).
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       HidePopupOnWindowEnterFullscreen) {
  EXPECT_TRUE(GenerateTestAutofillPopup(autofill_driver(), profile(),
                                        /*expect_popup_to_be_shown=*/true));

  // Enter fullscreen, which should cause the popup to hide.
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  content::WebContentsDelegate* wcd = browser();
  wcd->EnterFullscreenModeForTab(main_rfh(), {});
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  autofill_external_delegate().WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate().popup_hidden());
}

// Tests that exiting fullscreen hides the popup and, in particular, does not
// crash (crbug.com/1267047).
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       HidePopupOnWindowExitFullscreen) {
  content::WebContentsDelegate* wcd = browser();
  wcd->EnterFullscreenModeForTab(main_rfh(), {});

  EXPECT_TRUE(GenerateTestAutofillPopup(autofill_driver(), profile(),
                                        /*expect_popup_to_be_shown=*/true));

  // Exit fullscreen, which should cause the popup to hide.
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  wcd->ExitFullscreenModeForTab(web_contents());
  ASSERT_FALSE(browser()->window()->IsFullscreen());

  autofill_external_delegate().WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate().popup_hidden());
}

// This test checks that the browser doesn't crash if the delegate is deleted
// before the popup is hidden.
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       DeleteDelegateBeforePopupHidden) {
  EXPECT_TRUE(GenerateTestAutofillPopup(autofill_driver(), profile(),
                                        /*expect_popup_to_be_shown=*/true));

  // Delete the external delegate here so that is gets deleted before popup is
  // hidden. This can happen if the web_contents are destroyed before the popup
  // is hidden. See http://crbug.com/232475.
  // To do that, simulate that the RFH is deleted. This causes driver deletion,
  // which deletes the AutofillManager, which deletes the ExternalDelegate.
  ContentAutofillDriverFactory::FromWebContents(web_contents())
      ->RenderFrameDeleted(main_rfh());
}

// crbug.com/965025
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest, ResetSelectedLine) {
  EXPECT_TRUE(GenerateTestAutofillPopup(autofill_driver(), profile(),
                                        /*expect_popup_to_be_shown=*/true));

  auto* client =
      autofill::ChromeAutofillClient::FromWebContentsForTesting(web_contents());
  base::WeakPtr<AutofillSuggestionController> controller =
      client->suggestion_controller_for_testing();
  ASSERT_TRUE(controller);

  // Push some suggestions and select the line #3.
  std::vector<SelectOption> rows = {{u"suggestion1", u"suggestion1"},
                                    {u"suggestion2", u"suggestion2"},
                                    {u"suggestion3", u"suggestion3"},
                                    {u"suggestion4", u"suggestion4"}};
  client->UpdateAutofillDataListValues(rows);
  int original_suggestions_count = controller->GetLineCount();
  static_cast<AutofillPopupController&>(*controller).SelectSuggestion(3);

  // Replace the list with the smaller one.
  rows = {{u"suggestion1", u"suggestion1"}};
  client->UpdateAutofillDataListValues(rows);
  // Make sure that previously selected line #3 doesn't exist.
  ASSERT_LT(controller->GetLineCount(), original_suggestions_count);
  // Selecting a new line should not crash.
  static_cast<AutofillPopupController&>(*controller).SelectSuggestion(0);
}

}  // namespace autofill
