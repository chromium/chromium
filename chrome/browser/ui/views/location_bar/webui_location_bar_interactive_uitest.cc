// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/tracked_element/interaction_test_util_web_ui.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClassicPopupWebViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);

const WebContentsInteractionTestUtil::DeepQuery kOmniboxInputDeepQuery = {
    "toolbar-app", "location-bar", "readonly-omnibox", "#textInput"};

}  // namespace

class WebUILocationBarInteractiveUiTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  WebUILocationBarInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUILocationBar},
        {});
    // EnablePixelOutput();
  }
  ~WebUILocationBarInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    test_util().AddSimulator(
        std::make_unique<ui::InteractionTestUtilSimulatorWebUI>());

    // Insert an interceptor for network requests, so autocomplete doesn't
    // go off searching google.com.
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(&WebUILocationBarInteractiveUiTest::HandleRequest));

    // Wait for the toolbar to load. Note that we can't wait for the widget to
    // become visible instead because the Widget will always be visible on Mac
    // OS.
    ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->IsShowPending();
    }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  views::WebView* GetToolbarWebView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetWebUIToolbarViewForTesting()
        ->GetWebViewForTesting();
  }

  auto GetActiveClassicPopupWebView() {
    return base::BindLambdaForTesting([this]() -> views::View* {
      WebUILocationBar* location_bar = static_cast<WebUILocationBar*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar());
      return location_bar->GetOmniboxPopupViewForTesting()
          ->presenter()
          ->GetWebUIContent();
    });
  }

  auto WaitForClassicPopupReady() {
    return Steps(
        InAnyContext(
            WaitForShow(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
        InAnyContext(InstrumentNonTabWebView(kClassicPopupWebViewId,
                                             GetActiveClassicPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kClassicPopupWebViewId, GURL(chrome::kChromeUIOmniboxPopupURL))));
  }

  auto RemoveFocusFromPopup() {
    return Steps(InAnyContext(MoveMouseTo(kToolbarAppMenuButtonElementId)),
                 InSameContext(ClickMouse()),
                 InAnyContext(WaitForHide(
                     OmniboxPopupPresenterBase::kRoundedResultsFrame)));
  }

 private:
  static bool HandleRequest(
      content::URLLoaderInterceptor::RequestParams* params) {
    constexpr std::string_view headers =
        "HTTP/1.1 404 Not found\nContent-Type: application/json\n\n";

    content::URLLoaderInterceptor::WriteResponse(headers, "",
                                                 params->client.get());
    return false;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// Show and hide the omnibox popup.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, ShowHidePopup) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      // Shouldn't have a popup visible yet.
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Type some text, it should show up.
      EnterText(kOmniboxElementId, u"input"), WaitForClassicPopupReady(),
      // Removing the focus should hide the popup.
      RemoveFocusFromPopup());
}
