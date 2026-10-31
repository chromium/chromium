// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {

// Returns the input of the last result that the classic popup page received.
constexpr char kPageResultInputScript[] = R"(
  (() => {
    const result = document.querySelector('omnibox-popup-app').result_;
    return result ? result.input : '';
  })();
)";

}  // namespace

// Tests moving the classic WebUI popup's WebContents from one browser window's
// presenter to another's.
class OmniboxPopupPresenterMoveBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxPopupPresenterMoveBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::internal::kWebUIOmniboxPopup},
                                   {features::kWebUILocationBar});
  }

 protected:
  static LocationBarView* GetLocationBar(BrowserWindowInterface* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->location_bar_view();
  }

  static OmniboxPopupPresenterBase* GetPresenter(
      BrowserWindowInterface* browser) {
    return GetLocationBar(browser)->GetOmniboxPopupView()->presenter();
  }

  // Runs an omnibox query in `browser` whose synchronous matches open the
  // suggestion popup.
  static void StartAutocomplete(BrowserWindowInterface* browser,
                                const std::u16string& text) {
    OmniboxController* controller =
        GetLocationBar(browser)->GetOmniboxController();
    controller->edit_model()->SetUserText(text);
    AutocompleteInput input(
        text, metrics::OmniboxEventProto::BLANK,
        ChromeAutocompleteSchemeClassifier(browser->GetProfile()));
    input.set_omit_asynchronous_matches(true);
    controller->StartAutocomplete(input);
  }

  static void ClosePopup(BrowserWindowInterface* browser) {
    GetLocationBar(browser)->GetOmniboxController()->StopAutocomplete(
        /*clear_result=*/true);
  }

  static content::WebContents* GetPopupContents(
      BrowserWindowInterface* browser) {
    OmniboxPopupWebUIBaseContent* content =
        GetPresenter(browser)->GetWebUIContent();
    return content ? content->GetWrappedWebContents() : nullptr;
  }

  // Opens and closes the suggestion popup of `browser()` and waits until its
  // page has bound its handlers. Returns the popup's WebContents.
  content::WebContents* CreateLoadedPopupContents() {
    StartAutocomplete(browser(), u"foo");
    content::WebContents* contents = GetPopupContents(browser());
    CHECK(contents);
    CHECK(content::WaitForLoadStop(contents));
    OmniboxPopupUI* popup_ui =
        contents->GetWebUI()->GetController()->GetAs<OmniboxPopupUI>();
    CHECK(base::test::RunUntil([&]() {
      return popup_ui->omnibox_handler() && popup_ui->popup_handler();
    }));
    ClosePopup(browser());
    return contents;
  }

  static void MovePopup(BrowserWindowInterface* from,
                        BrowserWindowInterface* to) {
    GetPresenter(to)->AdoptWebUIContents(
        GetPresenter(from)->ReleaseWebUIContents());
  }

  static std::string GetPageResultInput(content::WebContents* contents) {
    return content::EvalJs(contents, kPageResultInputScript).ExtractString();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// The other window shows the moved popup without loading a new page, and the
// window that released it has no content left.
IN_PROC_BROWSER_TEST_F(OmniboxPopupPresenterMoveBrowserTest,
                       OtherWindowShowsMovedPopup) {
  content::WebContents* contents = CreateLoadedPopupContents();
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());

  MovePopup(browser(), other_browser);

  EXPECT_FALSE(GetPresenter(browser())->GetWebUIContent());
  EXPECT_EQ(GetPopupContents(other_browser), contents);

  StartAutocomplete(other_browser, u"bar");
  EXPECT_TRUE(GetPresenter(other_browser)->IsShown());
  EXPECT_EQ(GetPopupContents(other_browser), contents);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "bar"; }));
}

// The window that released its popup creates a new one when it next needs
// it.
IN_PROC_BROWSER_TEST_F(OmniboxPopupPresenterMoveBrowserTest,
                       OldWindowCreatesNewPopup) {
  content::WebContents* contents = CreateLoadedPopupContents();
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  MovePopup(browser(), other_browser);

  StartAutocomplete(browser(), u"bar");

  content::WebContents* new_contents = GetPopupContents(browser());
  ASSERT_TRUE(new_contents);
  EXPECT_NE(new_contents, contents);
  EXPECT_EQ(GetPopupContents(other_browser), contents);
}

// Closing the window that created the popup keeps the moved popup working in
// the other window.
IN_PROC_BROWSER_TEST_F(OmniboxPopupPresenterMoveBrowserTest,
                       MovedPopupOutlivesOldWindow) {
  content::WebContents* contents = CreateLoadedPopupContents();
  base::WeakPtr<content::WebContents> weak_contents = contents->GetWeakPtr();
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  MovePopup(browser(), other_browser);

  CloseBrowserSynchronously(browser());

  ASSERT_TRUE(weak_contents);
  StartAutocomplete(other_browser, u"bar");
  EXPECT_EQ(GetPopupContents(other_browser), contents);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "bar"; }));
}

// A popup can move back to the window that created it.
IN_PROC_BROWSER_TEST_F(OmniboxPopupPresenterMoveBrowserTest, MovesBack) {
  content::WebContents* contents = CreateLoadedPopupContents();
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  MovePopup(browser(), other_browser);

  MovePopup(other_browser, browser());

  EXPECT_FALSE(GetPresenter(other_browser)->GetWebUIContent());
  StartAutocomplete(browser(), u"bar");
  EXPECT_EQ(GetPopupContents(browser()), contents);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "bar"; }));
}
