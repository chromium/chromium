// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_client.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_factory.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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
  static LocationBar* GetLocationBar(BrowserWindowInterface* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)->GetLocationBar();
  }

  static OmniboxPopupHost* GetPopupHost(BrowserWindowInterface* browser) {
    return GetLocationBar(browser)->GetOmniboxPopupView()->popup_host();
  }

  // For presenter-only state.
  static OmniboxPopupPresenterBase* GetPresenter(
      BrowserWindowInterface* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (LocationBarView* view = browser_view->toolbar()->location_bar_view()) {
      return view->GetOmniboxPopupPresenterForTesting();
    }
    return static_cast<WebUILocationBar*>(browser_view->GetLocationBar())
        ->GetOmniboxPopupPresenterForTesting();
  }

  // Runs an omnibox query in `browser` whose synchronous matches open the
  // suggestion popup.
  static void StartAutocomplete(BrowserWindowInterface* browser,
                                const std::u16string& text) {
    // Leaves nothing focused, so that the window deactivation that follows
    // `CreateBrowser()` on some platforms cannot blur the omnibox and close
    // the popup.
    BrowserView::GetBrowserViewForBrowser(browser)
        ->GetFocusManager()
        ->SetFocusedView(nullptr);
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
        GetPopupHost(browser)->GetWebUIContent();
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

  // Runs the tasks posted so far.
  static void FlushPostedTasks() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Opens another window of the profile and waits until the window manager
  // has activated it. Window managers that activate a new window
  // asynchronously, e.g. Mutter on Wayland, would otherwise deactivate the
  // other windows later in the test, closing their popups, or override the
  // test's own `ActivateBrowser()` calls, which makes the new window take idle
  // popups.
  BrowserWindowInterface* CreateActiveBrowser() {
    BrowserWindowInterface* new_browser =
        CreateBrowser(browser()->GetProfile());
    ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);
    return new_browser;
  }

  // Notifies the browser window's observers that it became active. Some
  // window managers (e.g. Mutter on Wayland) ignore programmatic activation.
  static void ActivateBrowser(BrowserWindowInterface* browser) {
    BrowserView::GetBrowserViewForBrowser(browser)
        ->GetWidget()
        ->OnNativeWidgetActivationChanged(/*active=*/true);
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
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  StartAutocomplete(other_browser, u"bar");

  EXPECT_TRUE(GetPopupHost(other_browser)->IsShown());
  EXPECT_EQ(GetPopupContents(other_browser), contents);
  EXPECT_FALSE(GetPopupHost(browser())->GetWebUIContent());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "bar"; }));
}

// A popup that is shown stays in its window; the other window creates its
// own.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       DoesNotTakeShownPopup) {
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  content::WebContents* contents = OpenLoadedPopup(browser());
  ASSERT_TRUE(GetPopupHost(browser())->IsShown());

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
      GetPopupHost(browser())->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  EXPECT_FALSE(GetPresenter(browser())->CanReleaseWebUIContents());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

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
  ui_test_utils::WaitUntilBrowserBecomeActive(incognito_browser);

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
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  StartAutocomplete(other_browser, u"bar");
  ClosePopup(other_browser);

  StartAutocomplete(browser(), u"baz");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  EXPECT_FALSE(GetPopupHost(other_browser)->GetWebUIContent());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "baz"; }));
}

// Closing the window that holds the shared popup leaves the other windows
// working; the next one that needs a popup creates it.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       ClosingHolderWindowCreatesNewPopup) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
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

// Hiding a popup destroys the idle copy that another window created while
// that popup was busy, and keeps a copy that is still shown.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       HidingPopupDropsOtherIdleCopies) {
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  content::WebContents* contents = OpenLoadedPopup(browser());
  content::WebContents* other_contents = OpenLoadedPopup(other_browser);
  ASSERT_NE(other_contents, contents);

  // The first window's popup is still shown, so it stays.
  ClosePopup(other_browser);
  FlushPostedTasks();
  EXPECT_EQ(GetPopupContents(browser()), contents);
  EXPECT_EQ(GetPopupContents(other_browser), other_contents);

  // Now the other window's copy is idle and goes away.
  ClosePopup(browser());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !GetPopupHost(other_browser)->GetWebUIContent(); }));
  EXPECT_EQ(GetPopupContents(browser()), contents);
}

// A window that becomes active takes the idle popup of another window, so
// that the popup is in place before the user types.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       ActivationTakesIdlePopup) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  ActivateBrowser(other_browser);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPopupContents(other_browser) == contents; }));
  EXPECT_FALSE(GetPopupHost(browser())->GetWebUIContent());

  ActivateBrowser(browser());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPopupContents(browser()) == contents; }));
  EXPECT_FALSE(GetPopupHost(other_browser)->GetWebUIContent());
}

// A window that becomes active leaves a popup that is not idle where it is.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       ActivationLeavesBusyPopup) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  std::unique_ptr<OmniboxPopupDeactivationBlocker> blocker =
      GetPopupHost(browser())->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  ActivateBrowser(other_browser);
  // Runs after the task that the activation posted.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(GetPopupContents(browser()), contents);
  EXPECT_FALSE(GetPopupHost(other_browser)->GetWebUIContent());
}

// A popup whose page is still loading stays in its window.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       DoesNotTakeLoadingPopup) {
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  StartAutocomplete(browser(), u"foo");
  content::WebContents* contents = GetPopupContents(browser());
  ASSERT_TRUE(contents);
  ClosePopup(browser());
  ASSERT_TRUE(contents->IsLoading());

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  EXPECT_NE(GetPopupContents(other_browser), contents);
}

// A popup whose renderer crashed stays in its window; the other window loads a
// new page.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       DoesNotTakeCrashedPopup) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  content::CrashTab(contents);
  ASSERT_TRUE(contents->IsCrashed());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  content::WebContents* other_contents = GetPopupContents(other_browser);
  ASSERT_TRUE(other_contents);
  EXPECT_NE(other_contents, contents);
}

// Closing the last window of an incognito profile destroys its pool cleanly,
// and the regular profile keeps sharing.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       IncognitoPoolGoesAwayWithItsWindows) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::WaitUntilBrowserBecomeActive(incognito_browser);
  OpenLoadedPopup(incognito_browser);
  ClosePopup(incognito_browser);
  CloseBrowserSynchronously(incognito_browser);

  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  StartAutocomplete(other_browser, u"bar");
  EXPECT_EQ(GetPopupContents(other_browser), contents);
}

// Tests sharing with the WebUI location bar, which builds its own presenters.
class OmniboxPopupShareAcrossWindowsWebUILocationBarBrowserTest
    : public OmniboxPopupShareAcrossWindowsBrowserTest {
 public:
  OmniboxPopupShareAcrossWindowsWebUILocationBarBrowserTest() {
    location_bar_feature_list_.InitAndEnableFeature(
        features::kWebUILocationBar);
  }

 private:
  base::test::ScopedFeatureList location_bar_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OmniboxPopupShareAcrossWindowsWebUILocationBarBrowserTest,
    ShowTakesIdlePopupFromOtherWindow) {
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(other_browser), contents);
  EXPECT_FALSE(GetPopupHost(browser())->GetWebUIContent());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "bar"; }));
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

  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  // The new window may already have taken the popup on activation, but it
  // never creates its own.
  content::WebContents* other_contents = GetPopupContents(other_browser);
  EXPECT_TRUE(!other_contents || other_contents == contents);

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(other_browser), contents);
  EXPECT_FALSE(GetPopupHost(browser())->GetWebUIContent());
}

// Restoring several windows prewarms one popup, not one per window.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsEagerBrowserTest,
                       SeveralWindowsPrewarmOnePopup) {
  content::WebContents* contents = GetPopupContents(browser());
  ASSERT_TRUE(contents);
  BrowserWindowInterface* second_browser = CreateActiveBrowser();
  BrowserWindowInterface* third_browser = CreateActiveBrowser();

  // A window that becomes active may take the first window's popup, but no
  // window creates its own.
  for (BrowserWindowInterface* window :
       {browser(), second_browser, third_browser}) {
    content::WebContents* window_contents = GetPopupContents(window);
    EXPECT_TRUE(!window_contents || window_contents == contents);
  }
}

// Tests sharing when windows prewarm their popup after their first frame.
class OmniboxPopupShareAcrossWindowsDeferredBrowserTest
    : public OmniboxPopupShareAcrossWindowsBrowserTest {
 public:
  OmniboxPopupShareAcrossWindowsDeferredBrowserTest()
      : create_popup_webui_on_demand_(
            omnibox::SetCreatePopupWebUIOnDemandForTesting(false)) {
    defer_feature_list_.InitAndEnableFeature(
        omnibox::kOmniboxWebUIPopupDeferCreation);
  }

 private:
  base::AutoReset<bool> create_popup_webui_on_demand_;
  base::test::ScopedFeatureList defer_feature_list_;
};

// A window skips prewarming after its first frame when another window of the
// profile already has a popup.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsDeferredBrowserTest,
                       SkipsPrewarmAfterFirstFrame) {
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetPopupContents(browser()) != nullptr; }));
  content::WebContents* contents = GetPopupContents(browser());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  GetPresenter(other_browser)->OnBrowserWindowPresented();
  // Runs after the creation task that the call above posts.
  FlushPostedTasks();

  // The new window may have taken the first window's popup when it became
  // active, but it doesn't create its own.
  content::WebContents* other_contents = GetPopupContents(other_browser);
  EXPECT_TRUE(!other_contents || other_contents == contents);
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
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  StartAutocomplete(other_browser, u"bar");

  EXPECT_EQ(GetPopupContents(browser()), contents);
  content::WebContents* other_contents = GetPopupContents(other_browser);
  ASSERT_TRUE(other_contents);
  EXPECT_NE(other_contents, contents);
}

// Dropping the idle popups, as memory pressure does, destroys an idle popup
// in an inactive window and keeps a shown one.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       DropIdlePopupsKeepsShownPopup) {
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  content::WebContents* contents = OpenLoadedPopup(browser());
  OpenLoadedPopup(other_browser);
  ClosePopup(other_browser);
  ActivateBrowser(browser());
  FlushPostedTasks();
  ASSERT_TRUE(GetPresenter(other_browser)->CanReleaseWebUIContents());

  OmniboxPopupWebUIPoolFactory::GetForProfile(browser()->GetProfile())
      ->DropIdleWebUIContents();

  EXPECT_FALSE(GetPopupHost(other_browser)->GetWebUIContent());
  EXPECT_EQ(GetPopupContents(browser()), contents);
}

// Tests dropping idle popups under memory pressure with a timeout for the
// popup of the window that was activated last.
class OmniboxPopupShareAcrossWindowsIdleTimeoutBrowserTest
    : public OmniboxPopupShareAcrossWindowsBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  OmniboxPopupShareAcrossWindowsIdleTimeoutBrowserTest() {
    timeout_feature_list_.InitAndEnableFeatureWithParameters(
        omnibox::kOmniboxWebUIPopupShareAcrossWindows,
        {{omnibox::kOmniboxWebUIPopupShareActiveWindowPopupIdleTimeout.name,
          KeepsActiveWindowPopup() ? "inf" : "0s"}});
  }

 protected:
  bool KeepsActiveWindowPopup() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList timeout_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxPopupShareAcrossWindowsIdleTimeoutBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoTimeout" : "ZeroTimeout";
                         });

// Without a timeout, dropping the idle popups keeps the idle classic popup of
// the window that was activated last. With one that has passed, it drops it.
IN_PROC_BROWSER_TEST_P(OmniboxPopupShareAcrossWindowsIdleTimeoutBrowserTest,
                       DropIdlePopupsActiveWindowPopup) {
  // Created first, so that its activation can't take the popup below.
  CreateActiveBrowser();
  content::WebContents* contents = OpenLoadedPopup(browser());
  ClosePopup(browser());
  ActivateBrowser(browser());
  FlushPostedTasks();
  ASSERT_EQ(GetPopupContents(browser()), contents);
  ASSERT_TRUE(GetPresenter(browser())->CanReleaseWebUIContents());

  OmniboxPopupWebUIPoolFactory::GetForProfile(browser()->GetProfile())
      ->DropIdleWebUIContents();

  EXPECT_EQ(GetPopupContents(browser()),
            KeepsActiveWindowPopup() ? contents : nullptr);
}

// Records where each window's classic popup came from and how long a move
// took.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       RecordsContentSourceAndMoveDuration) {
  base::HistogramTester histograms;
  OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  StartAutocomplete(other_browser, u"bar");

  constexpr char kSource[] = "Omnibox.Popup.WebUI.Shared.ContentSource";
  histograms.ExpectBucketCount(
      kSource, OmniboxPopupWebUIPoolClient::ContentSource::kCreated, 1);
  // The new window takes the popup on activation or when it shows it.
  EXPECT_EQ(
      histograms.GetBucketCount(
          kSource, OmniboxPopupWebUIPoolClient::ContentSource::kTakenOnDemand) +
          histograms.GetBucketCount(
              kSource,
              OmniboxPopupWebUIPoolClient::ContentSource::kTakenOnActivation),
      1);
  histograms.ExpectTotalCount("Omnibox.Popup.WebUI.Shared.MoveDuration", 1);
}

// A popup created while another window's popup is busy is recorded as such.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       RecordsCreatedWhileOtherBusy) {
  OpenLoadedPopup(browser());
  ClosePopup(browser());
  // Keeps the first window's popup busy.
  std::unique_ptr<OmniboxPopupDeactivationBlocker> blocker =
      GetPresenter(browser())->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  ASSERT_FALSE(GetPresenter(other_browser)->GetWebUIContent());
  base::HistogramTester histograms;

  // Creates the content without showing it, since only its source matters.
  ASSERT_TRUE(GetPresenter(other_browser)
                  ->GetOrCreateWebUIContent(
                      OmniboxPopupPresenterBase::CreationReason::kShow));

  constexpr char kSource[] = "Omnibox.Popup.WebUI.Shared.ContentSource";
  histograms.ExpectBucketCount(
      kSource,
      OmniboxPopupWebUIPoolClient::ContentSource::kCreatedWhileOtherBusy, 1);
  histograms.ExpectBucketCount(
      kSource, OmniboxPopupWebUIPoolClient::ContentSource::kCreated, 0);
}

// A popup created after the window that held the shared popup closed with it
// is recorded as such.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       RecordsCreatedAfterHolderClosed) {
  OpenLoadedPopup(browser());
  ClosePopup(browser());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  StartAutocomplete(other_browser, u"bar");
  ClosePopup(other_browser);
  ASSERT_FALSE(GetPresenter(browser())->GetWebUIContent());
  // Keeps the first window from taking the popup when it becomes active as
  // the other window closes, so the popup closes with its window.
  std::unique_ptr<OmniboxPopupDeactivationBlocker> blocker =
      GetPresenter(other_browser)->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  base::HistogramTester histograms;

  CloseBrowserSynchronously(other_browser);
  blocker.reset();
  // Creates the content without showing it, since only its source matters.
  ASSERT_TRUE(GetPresenter(browser())->GetOrCreateWebUIContent(
      OmniboxPopupPresenterBase::CreationReason::kShow));

  histograms.ExpectUniqueSample(
      "Omnibox.Popup.WebUI.Shared.ContentSource",
      OmniboxPopupWebUIPoolClient::ContentSource::kCreatedAfterHolderClosed, 1);
}

// Hiding a popup records how many idle copies in other windows it destroyed.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareAcrossWindowsBrowserTest,
                       RecordsCopiesDroppedOnHide) {
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  OpenLoadedPopup(browser());
  OpenLoadedPopup(other_browser);
  ClosePopup(other_browser);
  FlushPostedTasks();
  base::HistogramTester histograms;

  ClosePopup(browser());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !GetPresenter(other_browser)->GetWebUIContent(); }));

  histograms.ExpectUniqueSample(
      "Omnibox.Popup.WebUI.Shared.CopiesDroppedOnHide", 1, 1);
}

// Tests that browser windows of a profile share the full WebUI popup.
class OmniboxPopupShareFullPopupAcrossWindowsBrowserTest
    : public OmniboxPopupShareAcrossWindowsBrowserTest {
 public:
  OmniboxPopupShareFullPopupAcrossWindowsBrowserTest() {
    full_popup_feature_list_.InitAndEnableFeature(
        omnibox::internal::kWebUIOmniboxFullPopup);
  }

 protected:
  static LocationBarView* GetLocationBarView(BrowserWindowInterface* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->location_bar_view();
  }

  // Hides the loaded full popup of `browser` and waits until it is idle, which
  // takes the reply to the `ClearPopup()` that hiding sends. Returns the
  // popup's WebContents.
  static content::WebContents* GetIdleFullPopup(
      BrowserWindowInterface* browser) {
    content::WebContents* contents = GetPopupContents(browser);
    CHECK(contents);
    WaitForPopupPage(contents);
    GetPresenter(browser)->Hide();
    CHECK(base::test::RunUntil(
        [&]() { return GetPresenter(browser)->CanReleaseWebUIContents(); }));
    return contents;
  }

  // Opens a second window that skipped loading its full popup because the
  // first window has one, and returns it. The first window keeps its popup,
  // and the second has none.
  BrowserWindowInterface* CreateBrowserWithSkippedFullPopup() {
    GetIdleFullPopup(browser());
    // Keeps the new window from taking the popup when it becomes active.
    std::unique_ptr<OmniboxPopupDeactivationBlocker> blocker =
        GetPresenter(browser())->CreateDeactivationBlocker();
    CHECK(blocker);
    BrowserWindowInterface* other_browser = CreateActiveBrowser();
    DropPopup(other_browser);
    return other_browser;
  }

  // Destroys the popup of `browser`, if it has one. A window that skipped
  // loading its full popup creates one if its omnibox is focused as it opens
  // while the other window's popup is busy.
  static void DropPopup(BrowserWindowInterface* browser) {
    if (GetPresenter(browser)->GetWebUIContent()) {
      // Destroys the released WebContents.
      GetPresenter(browser)->ReleaseWebUIContents();
    }
  }

  // Replaces the hidden full popup of `browser` with a new one whose page is
  // still loading, and returns the new popup's WebContents.
  static content::WebContents* ReplaceWithLoadingFullPopup(
      BrowserWindowInterface* browser) {
    // Destroys the released WebContents.
    GetPresenter(browser)->ReleaseWebUIContents();
    CHECK(GetPresenter(browser)->GetOrCreateWebUIContent(
        OmniboxPopupPresenterBase::CreationReason::kShow));
    content::WebContents* contents = GetPopupContents(browser);
    CHECK(contents->IsLoading());
    return contents;
  }

 private:
  base::test::ScopedFeatureList full_popup_feature_list_;
};

// Tests the full WebUI popup with the full WebUI handler, which is bound to
// the window that loaded the page.
class OmniboxPopupShareFullHandlerBrowserTest
    : public OmniboxPopupShareFullPopupAcrossWindowsBrowserTest {
 public:
  OmniboxPopupShareFullHandlerBrowserTest() {
    full_handler_feature_list_.InitAndEnableFeature(
        omnibox::kWebUISearchboxWithoutModelController);
  }

 private:
  base::test::ScopedFeatureList full_handler_feature_list_;
};

// Each window keeps its own full popup, since the full handler can't move.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullHandlerBrowserTest,
                       EachWindowKeepsItsOwnFullPopup) {
  EXPECT_EQ(GetPresenter(browser())->GetSharedPopupKind(),
            OmniboxPopupState::kNone);
  content::WebContents* contents = GetPopupContents(browser());
  ASSERT_TRUE(contents);

  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  content::WebContents* other_contents = GetPopupContents(other_browser);
  ASSERT_TRUE(other_contents);
  EXPECT_NE(other_contents, contents);
  EXPECT_EQ(GetPopupContents(browser()), contents);
}

// A new window loads no full popup of its own while another window has one,
// and takes that popup once it is idle.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       NewWindowTakesIdleFullPopup) {
  EXPECT_EQ(GetPresenter(browser())->GetSharedPopupKind(),
            OmniboxPopupState::kFull);
  content::WebContents* contents = GetIdleFullPopup(browser());

  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  // The new window either skipped creating its popup or already took it, if
  // it showed the popup for its focused omnibox.
  content::WebContents* other_contents = GetPopupContents(other_browser);
  EXPECT_TRUE(!other_contents || other_contents == contents);

  ASSERT_TRUE(GetPresenter(other_browser)
                  ->GetOrCreateWebUIContent(
                      OmniboxPopupPresenterBase::CreationReason::kShow));
  EXPECT_EQ(GetPopupContents(other_browser), contents);
  EXPECT_FALSE(GetPresenter(browser())->GetWebUIContent());
}

// A draft in the omnibox keeps the full popup in its window.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       DraftKeepsFullPopup) {
  GetIdleFullPopup(browser());

  GetLocationBar(browser())->GetOmniboxController()->edit_model()->SetUserText(
      u"draft");

  ASSERT_FALSE(GetPresenter(browser())->IsShown());
  EXPECT_FALSE(GetPresenter(browser())->CanReleaseWebUIContents());
}

// The full popup stays in its window while a `ClearPopup()` reply is pending.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       PendingClearKeepsFullPopup) {
  GetIdleFullPopup(browser());

  GetPresenter(browser())->GetWebUIContent()->Clear();

  EXPECT_FALSE(GetPresenter(browser())->CanReleaseWebUIContents());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPresenter(browser())->CanReleaseWebUIContents(); }));
}

// A window that skipped loading its full popup creates its own when its
// omnibox is focused while another window's popup is busy, since the full popup
// is only shown once its page is ready.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       SkippedPrewarmCreatesFullPopupOnFocusWhileOtherBusy) {
  content::WebContents* contents = GetIdleFullPopup(browser());
  // Keeps the first window's popup busy.
  std::unique_ptr<OmniboxPopupDeactivationBlocker> blocker =
      GetPresenter(browser())->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  BrowserWindowInterface* other_browser = CreateActiveBrowser();

  // The new window may already have focused its omnibox when it opened.
  GetLocationBarView(other_browser)->OnOmniboxFocused();

  content::WebContents* other_contents = GetPopupContents(other_browser);
  ASSERT_TRUE(other_contents);
  EXPECT_NE(other_contents, contents);
  EXPECT_EQ(GetPopupContents(browser()), contents);
}

// A window that skipped loading its full popup waits when its omnibox is
// focused while another window's popup is still loading, and takes that popup
// once it has loaded.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       SkippedPrewarmTakesLoadingFullPopupOnceLoaded) {
  BrowserWindowInterface* other_browser = CreateBrowserWithSkippedFullPopup();
  content::WebContents* contents = ReplaceWithLoadingFullPopup(browser());
  ActivateBrowser(other_browser);

  GetLocationBarView(other_browser)->OnOmniboxFocused();

  EXPECT_FALSE(GetPopupContents(other_browser));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPopupContents(other_browser) == contents; }));
  EXPECT_FALSE(GetPresenter(browser())->GetWebUIContent());
}

// A window waiting for another window's loading full popup creates its own if
// that window closes first.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       SkippedPrewarmCreatesFullPopupWhenLoadingHolderCloses) {
  BrowserWindowInterface* other_browser = CreateBrowserWithSkippedFullPopup();
  ReplaceWithLoadingFullPopup(browser());
  ActivateBrowser(other_browser);
  GetLocationBarView(other_browser)->OnOmniboxFocused();
  ASSERT_FALSE(GetPopupContents(other_browser));

  CloseBrowserSynchronously(browser());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPopupContents(other_browser) != nullptr; }));
}

// A window that skipped loading its full popup creates its own when its
// omnibox is focused after the window that had the popup closed with it.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       SkippedPrewarmCreatesFullPopupOnFocusAfterHolderClosed) {
  GetIdleFullPopup(browser());
  // Keeps the other window from taking the popup when it becomes active as
  // the first window closes, so the popup closes with its window.
  std::unique_ptr<OmniboxPopupDeactivationBlocker> blocker =
      GetPresenter(browser())->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  DropPopup(other_browser);
  CloseBrowserSynchronously(browser());
  blocker.reset();

  GetLocationBarView(other_browser)->OnOmniboxFocused();

  EXPECT_TRUE(GetPopupContents(other_browser));
}

// A window that waits for a loading full popup in another window keeps
// waiting when the popup loads while a third window is active, and takes the
// popup once it is activated itself.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       InactiveWindowWaitsForActivationToTakeFullPopup) {
  BrowserWindowInterface* waiting_browser = CreateBrowserWithSkippedFullPopup();
  // Window managers activate a new window themselves, possibly after
  // `ActivateBrowser()`, so the active window is the newest one.
  BrowserWindowInterface* active_browser = CreateActiveBrowser();
  ActivateBrowser(active_browser);
  // Gives the active window a loaded popup of its own, which a blocker keeps
  // there, so that it has no request of its own for the next popup.
  OmniboxPopupWebUIBaseContent* active_content =
      GetPresenter(active_browser)
          ->GetOrCreateWebUIContent(
              OmniboxPopupPresenterBase::CreationReason::kShow);
  ASSERT_TRUE(active_content);
  ASSERT_TRUE(content::WaitForLoadStop(active_content->GetWebContents()));
  std::unique_ptr<OmniboxPopupDeactivationBlocker> blocker =
      GetPresenter(active_browser)->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  ASSERT_TRUE(GetPresenter(browser())->GetOrCreateWebUIContent(
      OmniboxPopupPresenterBase::CreationReason::kShow));
  content::WebContents* contents = GetPopupContents(browser());
  ASSERT_TRUE(contents->IsLoading());
  GetLocationBarView(waiting_browser)->OnOmniboxFocused();
  ASSERT_FALSE(GetPopupContents(waiting_browser));

  ASSERT_TRUE(content::WaitForLoadStop(contents));
  // Runs the task that the load posted to run a waiting client.
  FlushPostedTasks();
  EXPECT_FALSE(GetPopupContents(waiting_browser));

  ActivateBrowser(waiting_browser);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPopupContents(waiting_browser) == contents; }));
}

// A window whose own full popup another window took gets one again when its
// omnibox is focused.
IN_PROC_BROWSER_TEST_F(OmniboxPopupShareFullPopupAcrossWindowsBrowserTest,
                       WindowThatLostItsFullPopupGetsOneOnFocus) {
  content::WebContents* contents = GetIdleFullPopup(browser());
  BrowserWindowInterface* other_browser = CreateActiveBrowser();
  ASSERT_TRUE(GetPresenter(other_browser)
                  ->GetOrCreateWebUIContent(
                      OmniboxPopupPresenterBase::CreationReason::kShow));
  ASSERT_EQ(GetPopupContents(other_browser), contents);
  ASSERT_FALSE(GetPresenter(browser())->GetWebUIContent());

  GetLocationBarView(browser())->OnOmniboxFocused();

  EXPECT_TRUE(GetPopupContents(browser()));
}
