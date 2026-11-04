// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/views/focus/focus_manager.h"

namespace {

// Returns the input of the last result that the classic popup page received.
constexpr char kPageResultInputScript[] = R"(
  (() => {
    const result = document.querySelector('omnibox-popup-app').result_;
    return result ? result.input : '';
  })();
)";

}  // namespace

// Tests that browser windows of a profile share the classic WebUI popup with
// `omnibox::kOmniboxWebUIPopupShareAcrossWindows`.
class OmniboxPopupShareAcrossWindowsBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxPopupShareAcrossWindowsBrowserTest() {
    feature_list_.InitWithFeatures(
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::kOmniboxWebUIPopupShareAcrossWindows,
         omnibox::kOmniboxKeepOpenOnFileSelection},
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
    // Leaves nothing focused, so that the window deactivation that follows
    // `CreateBrowser()` on some platforms cannot blur the omnibox and close
    // the popup.
    GetLocationBar(browser)->GetFocusManager()->SetFocusedView(nullptr);
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

  // Waits until the popup page in `contents` has loaded and bound its
  // handlers.
  static void WaitForPopupPage(content::WebContents* contents) {
    CHECK(content::WaitForLoadStop(contents));
    OmniboxPopupUI* popup_ui =
        contents->GetWebUI()->GetController()->GetAs<OmniboxPopupUI>();
    CHECK(base::test::RunUntil([&]() {
      return popup_ui->omnibox_handler() && popup_ui->popup_handler();
    }));
  }

  // Opens the suggestion popup of `browser` and waits until its page has
  // bound its handlers. Returns the popup's WebContents.
  static content::WebContents* OpenLoadedPopup(
      BrowserWindowInterface* browser) {
    StartAutocomplete(browser, u"foo");
    content::WebContents* contents = GetPopupContents(browser);
    CHECK(contents);
    WaitForPopupPage(contents);
    return contents;
  }

  static std::string GetPageResultInput(content::WebContents* contents) {
    return content::EvalJs(contents, kPageResultInputScript).ExtractString();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Showing the popup in a second window moves the idle popup of the first
// window there instead of loading a new page.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       ShowTakesIdlePopupFromOtherWindow) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());

  StartAutocomplete(other_browser, u"bar");

  EXPECT_TRUE(GetPresenter(other_browser)->IsShown());
  EXPECT_EQ(GetPopupContents(other_browser), contents);
  EXPECT_FALSE(GetPresenter(browser())->GetWebUIContent());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "bar"; }));
}

// A popup that is shown stays in its window; the other window creates its
// own.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       DoesNotTakeShownPopup) {
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  content::WebContents* contents = OpenLoadedPopup(browser());
  ASSERT_TRUE(GetPresenter(browser())->IsShown());

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  content::WebContents* other_contents = GetPopupContents(other_browser);
  ASSERT_TRUE(other_contents);
  EXPECT_NE(other_contents, contents);
}

// A popup that holds a deactivation blocker, e.g. for an open file dialog,
// stays in its window.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       DoesNotTakePopupWithDeactivationBlocker) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  std::unique_ptr<OmniboxPopupDeactivationBlocker> blocker =
      GetPresenter(browser())->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  EXPECT_FALSE(GetPresenter(browser())->CanReleaseWebUIContents());
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  EXPECT_NE(GetPopupContents(other_browser), contents);
}

// Windows of different profiles never share a popup.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       DoesNotShareAcrossProfiles) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();

  StartAutocomplete(incognito_browser, u"bar");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  content::WebContents* incognito_contents =
      GetPopupContents(incognito_browser);
  ASSERT_TRUE(incognito_contents);
  EXPECT_NE(incognito_contents, contents);
}

// The window that gave its popup away takes it back when it needs it again.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       PopupMovesBack) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  StartAutocomplete(other_browser, u"bar");
  ClosePopup(other_browser);

  StartAutocomplete(browser(), u"baz");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  EXPECT_FALSE(GetPresenter(other_browser)->GetWebUIContent());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "baz"; }));
}

// Closing the window that holds the shared popup leaves the other windows
// working; the next one that needs a popup creates it.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       ClosingHolderWindowCreatesNewPopup) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  StartAutocomplete(other_browser, u"bar");
  ClosePopup(other_browser);
  ASSERT_EQ(GetPopupContents(other_browser), contents);

  CloseBrowserSynchronously(other_browser);
  StartAutocomplete(browser(), u"baz");

  content::WebContents* new_contents = GetPopupContents(browser());
  ASSERT_TRUE(new_contents);
  WaitForPopupPage(new_contents);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(new_contents) == "baz"; }));
}

// Tests sharing when windows prewarm their popup, as in
// production.
class OmniboxPopupShareAcrossWindowsEagerBrowserTest
    : public OmniboxPopupShareAcrossWindowsBrowserTest {
 public:
  OmniboxPopupShareAcrossWindowsEagerBrowserTest()
      : create_popup_webui_on_demand_(
            omnibox::SetCreatePopupWebUIOnDemandForTesting(false)) {
    feature_list_.InitAndDisableFeature(
        omnibox::kOmniboxWebUIPopupDeferCreation);
  }

 private:
  base::AutoReset<bool> create_popup_webui_on_demand_;
  base::test::ScopedFeatureList feature_list_;
};

// A new window skips prewarming its popup when another window of the profile
// already has one, and takes that popup when it first needs it.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsEagerBrowserTest,
                       SkipsPrewarm) {
  content::WebContents* contents = GetPopupContents(browser());
  ASSERT_TRUE(contents);
  WaitForPopupPage(contents);

  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  EXPECT_FALSE(GetPresenter(other_browser)->GetWebUIContent());

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(other_browser), contents);
  EXPECT_FALSE(GetPresenter(browser())->GetWebUIContent());
}

// Tests that windows do not share popups with the feature disabled.
class OmniboxPopupShareAcrossWindowsDisabledBrowserTest
    : public OmniboxPopupShareAcrossWindowsBrowserTest {
 public:
  OmniboxPopupShareAcrossWindowsDisabledBrowserTest() {
    disabled_feature_list_.InitAndDisableFeature(
        omnibox::kOmniboxWebUIPopupShareAcrossWindows);
  }

 private:
  base::test::ScopedFeatureList disabled_feature_list_;
};

// Each window creates its own popup and keeps it.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsDisabledBrowserTest,
                       EachWindowCreatesItsOwnPopup) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  content::WebContents* other_contents = GetPopupContents(other_browser);
  ASSERT_TRUE(other_contents);
  EXPECT_NE(other_contents, contents);
}
