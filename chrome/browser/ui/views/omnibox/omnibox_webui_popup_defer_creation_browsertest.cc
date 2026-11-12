// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"
#include "ui/compositor/compositor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Schedules a paint of `widget` and waits until it presents a frame.
[[nodiscard]] bool PaintAndWaitForPresentation(views::Widget* widget) {
  base::test::TestFuture<void> presented;
  widget->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
      base::IgnoreArgs<const viz::FrameTimingDetails&>(
          presented.GetCallback()));
  widget->GetRootView()->SchedulePaint();
  return presented.Wait();
}

}  // namespace

// Tests when the WebUI popup that shows omnibox suggestions creates its
// content.
class OmniboxWebUIPopupCreationBrowserTest : public InProcessBrowserTest {
 public:
  // Enables the WebUI suggestion popup.
  OmniboxWebUIPopupCreationBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::internal::kWebUIOmniboxPopup},
                                   {features::kWebUILocationBar});
  }

 protected:
  LocationBarView* location_bar() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

  OmniboxPopupHost* popup_host() {
    return location_bar()->GetOmniboxPopupView()->popup_host();
  }

  // Runs an omnibox query whose synchronous matches open the suggestion
  // popup, as typing does.
  void TypeAndWaitForPopup() {
    OmniboxController* controller = location_bar()->GetOmniboxController();
    controller->edit_model()->SetUserText(u"foo");
    AutocompleteInput input(
        u"foo", metrics::OmniboxEventProto::BLANK,
        ChromeAutocompleteSchemeClassifier(browser()->GetProfile()));
    input.set_omit_asynchronous_matches(true);
    controller->StartAutocomplete(input);
    ASSERT_TRUE(popup_host()->IsShown());
  }

  // Closes the suggestion popup by clearing the suggestions.
  void ClosePopup() {
    location_bar()->GetOmniboxController()->StopAutocomplete(
        /*clear_result=*/true);
    ASSERT_FALSE(popup_host()->IsShown());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests `omnibox::kOmniboxWebUIPopupDeferCreation`, which defers creating the
// popup's content until the browser window has painted.
class OmniboxWebUIPopupDeferCreationBrowserTest
    : public OmniboxWebUIPopupCreationBrowserTest {
 public:
  OmniboxWebUIPopupDeferCreationBrowserTest() {
    feature_list_.InitAndEnableFeature(
        omnibox::kOmniboxWebUIPopupDeferCreation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // Follow the deferral feature rather than the on-demand creation that
  // `InProcessBrowserTest` sets up.
  base::AutoReset<bool> create_popup_webui_on_demand_ =
      omnibox::SetCreatePopupWebUIOnDemandForTesting(false);
};

// The popup creates its content without user action once the browser window
// has presented, and keeps it hidden until it opens.
IN_PROC_BROWSER_TEST_F(OmniboxWebUIPopupDeferCreationBrowserTest,
                       CreatesHiddenContentAfterFirstPresentation) {
  ASSERT_TRUE(popup_host());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return popup_host()->GetWebUIContent() != nullptr; }));
  EXPECT_FALSE(popup_host()->IsShown());

  TypeAndWaitForPopup();
  EXPECT_TRUE(popup_host()->GetWebUIContent());
}

// Browser tests create the popup's content only when something needs it (see
// `InProcessBrowserTest`). Tests that opening the popup creates it.
class OmniboxWebUIPopupOnDemandCreationBrowserTest
    : public OmniboxWebUIPopupCreationBrowserTest {
 public:
  // Enables deferred creation to show that on-demand creation takes precedence.
  OmniboxWebUIPopupOnDemandCreationBrowserTest() {
    defer_creation_feature_.InitAndEnableFeature(
        omnibox::kOmniboxWebUIPopupDeferCreation);
  }

 private:
  base::test::ScopedFeatureList defer_creation_feature_;
  // Explicit, so that these tests don't depend on the browser test default.
  base::AutoReset<bool> on_demand_creation_ =
      omnibox::SetCreatePopupWebUIOnDemandForTesting(true);
};

// Typing before the content exists opens the popup, which creates the content
// and its widget.
IN_PROC_BROWSER_TEST_F(OmniboxWebUIPopupOnDemandCreationBrowserTest,
                       CreatesContentWhenPopupOpens) {
  ASSERT_TRUE(popup_host());
  EXPECT_FALSE(popup_host()->GetWebUIContent());

  TypeAndWaitForPopup();
  OmniboxPopupWebUIBaseContent* content = popup_host()->GetWebUIContent();
  ASSERT_TRUE(content);
  EXPECT_TRUE(content->GetWidget());
  EXPECT_TRUE(content::WaitForLoadStop(content->GetWrappedWebContents()));
}

// Tests `omnibox::kOmniboxWebUIPopupCreateOnFocus`: the content is created when
// the omnibox is first focused or the popup opens.
class OmniboxWebUIPopupCreateOnFocusBrowserTest
    : public OmniboxWebUIPopupDeferCreationBrowserTest {
 public:
  OmniboxWebUIPopupCreateOnFocusBrowserTest() {
    create_on_focus_feature_.InitAndEnableFeatureWithParameters(
        omnibox::kOmniboxWebUIPopupDeferCreation,
        {{omnibox::kOmniboxWebUIPopupCreateOnFocus.name, "true"}});
  }

 private:
  base::test::ScopedFeatureList create_on_focus_feature_;
};

// A new window that opens on a page doesn't focus its omnibox, so presenting
// it doesn't create the content. Focusing its omnibox does.
IN_PROC_BROWSER_TEST_F(OmniboxWebUIPopupCreateOnFocusBrowserTest,
                       CreatesContentOnFocus) {
  BrowserWindowInterface* new_browser = CreateBrowserWindow(
      BrowserWindowCreateParams(browser()->GetProfile(),
                                /*from_user_gesture=*/true));
  chrome::AddSelectedTabWithURL(new_browser, GURL("data:text/html,page"),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  new_browser->GetWindow()->Show();
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(new_browser);
  LocationBarView* new_location_bar =
      browser_view->toolbar()->location_bar_view();
  OmniboxPopupHost* new_host =
      new_location_bar->GetOmniboxPopupView()->popup_host();
  ASSERT_TRUE(new_host);
  ASSERT_TRUE(PaintAndWaitForPresentation(browser_view->GetWidget()));
  // A creation task posted by the first presentation would run while waiting
  // for the second one.
  ASSERT_TRUE(PaintAndWaitForPresentation(browser_view->GetWidget()));
  EXPECT_FALSE(new_host->GetWebUIContent());

  new_location_bar->OnOmniboxFocused();
  OmniboxPopupWebUIBaseContent* content = new_host->GetWebUIContent();
  ASSERT_TRUE(content);
  EXPECT_FALSE(new_host->IsShown());
  EXPECT_TRUE(content::WaitForLoadStop(content->GetWrappedWebContents()));
  CloseBrowserSynchronously(new_browser);
}

// Focusing the omnibox before the window has presented a frame, as opening a
// new tab window does, waits for the window's first presentation before
// creating the content.
IN_PROC_BROWSER_TEST_F(OmniboxWebUIPopupCreateOnFocusBrowserTest,
                       DefersEarlyFocusUntilWindowPresentation) {
  BrowserWindowInterface* new_browser = CreateBrowserWindow(
      BrowserWindowCreateParams(browser()->GetProfile(),
                                /*from_user_gesture=*/true));
  chrome::AddSelectedTabWithURL(new_browser, GURL("data:text/html,page"),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  new_browser->GetWindow()->Show();
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(new_browser);
  LocationBarView* new_location_bar =
      browser_view->toolbar()->location_bar_view();
  OmniboxPopupHost* new_host =
      new_location_bar->GetOmniboxPopupView()->popup_host();
  ASSERT_TRUE(new_host);

  new_location_bar->OnOmniboxFocused();
  EXPECT_FALSE(new_host->GetWebUIContent());

  ASSERT_TRUE(PaintAndWaitForPresentation(browser_view->GetWidget()));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return new_host->GetWebUIContent() != nullptr; }));
  EXPECT_FALSE(new_host->IsShown());
  EXPECT_TRUE(content::WaitForLoadStop(
      new_host->GetWebUIContent()->GetWrappedWebContents()));
  CloseBrowserSynchronously(new_browser);
}

// Uses the WebUI location bar, which `BrowserView::GetLocationBarView()` does
// not return.
class OmniboxWebUIPopupCreateOnFocusWebUILocationBarBrowserTest
    : public OmniboxWebUIPopupCreateOnFocusBrowserTest {
 public:
  OmniboxWebUIPopupCreateOnFocusWebUILocationBarBrowserTest() {
    webui_location_bar_features_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUILocationBar}, {});
  }

 private:
  base::test::ScopedFeatureList webui_location_bar_features_;
};

// Focus moving into the WebUI location bar creates the content, as focusing
// the Views omnibox does.
IN_PROC_BROWSER_TEST_F(
    OmniboxWebUIPopupCreateOnFocusWebUILocationBarBrowserTest,
    CreatesContentOnFocusWithin) {
  BrowserWindowInterface* new_browser = CreateBrowserWindow(
      BrowserWindowCreateParams(browser()->GetProfile(),
                                /*from_user_gesture=*/true));
  chrome::AddSelectedTabWithURL(new_browser, GURL("data:text/html,page"),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  new_browser->GetWindow()->Show();
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(new_browser);
  ASSERT_FALSE(browser_view->GetLocationBarView());
  auto* new_location_bar =
      static_cast<WebUILocationBar*>(browser_view->GetLocationBar());
  ASSERT_TRUE(new_location_bar);
  OmniboxPopupHost* new_host =
      new_location_bar->GetOmniboxPopupView()->popup_host();
  ASSERT_TRUE(new_host);
  ASSERT_TRUE(PaintAndWaitForPresentation(browser_view->GetWidget()));
  // A creation task posted by the first presentation would run while waiting
  // for the second one.
  ASSERT_TRUE(PaintAndWaitForPresentation(browser_view->GetWidget()));
  EXPECT_FALSE(new_host->GetWebUIContent());

  new_location_bar->SetFocusWithin(true);
  OmniboxPopupWebUIBaseContent* content = new_host->GetWebUIContent();
  ASSERT_TRUE(content);
  EXPECT_FALSE(new_host->IsShown());
  EXPECT_TRUE(content::WaitForLoadStop(content->GetWrappedWebContents()));
  CloseBrowserSynchronously(new_browser);
}
