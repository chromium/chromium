// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_and_views_toolbar_interactive_uitest_base.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/reload_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_reload_control.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/webview/webview.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

WebUIAndViewsToolbarInteractiveUiTestBase::
    WebUIAndViewsToolbarInteractiveUiTestBase() = default;
WebUIAndViewsToolbarInteractiveUiTestBase::
    ~WebUIAndViewsToolbarInteractiveUiTestBase() = default;

WebContentsInteractionTestUtil::DeepQuery
WebUIAndViewsToolbarInteractiveUiTestBase::WebUIReloadButtonDeepQuery() {
  CHECK(features::IsWebUIReloadButtonEnabled());
  return WebContentsInteractionTestUtil::DeepQuery(
      {"toolbar-app", "reload-button"});
}

WebContentsInteractionTestUtil::DeepQuery
WebUIAndViewsToolbarInteractiveUiTestBase::WebUIBackForwardButtonDeepQuery() {
  CHECK(features::IsWebUIBackForwardButtonEnabled());
  return WebContentsInteractionTestUtil::DeepQuery(
      {"toolbar-app", "back-forward-button"});
}

ui::ElementIdentifier WebUIAndViewsToolbarInteractiveUiTestBase::TabId() {
  return kTabId;
}

ui::ElementIdentifier
WebUIAndViewsToolbarInteractiveUiTestBase::WebUIToolbarId() {
  CHECK(features::IsWebUIToolbarEnabled());
  return kWebUIToolbarId;
}

views::WebView*
WebUIAndViewsToolbarInteractiveUiTestBase::GetWebUIToolbarWebView() {
  CHECK(features::IsWebUIToolbarEnabled());
  return ::GetWebUIToolbarWebView(browser())->GetWebViewForTesting();
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::WaitForToolbarLoaded() {
  return Steps(InstrumentToolbar(), WaitForWebContentsReady(WebUIToolbarId()));
}

ReloadControl& WebUIAndViewsToolbarInteractiveUiTestBase::GetReloadControl() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  ToolbarButtonProvider* provider = browser_view->toolbar_button_provider();
  return *provider->GetReloadButton();
}

WebUIReloadControl&
WebUIAndViewsToolbarInteractiveUiTestBase::GetWebUIReloadButton() {
  CHECK(features::IsWebUIReloadButtonEnabled());
  return static_cast<WebUIReloadControl&>(GetReloadControl());
}

ReloadButton&
WebUIAndViewsToolbarInteractiveUiTestBase::GetNonWebUIReloadButton() {
  CHECK(!features::IsWebUIReloadButtonEnabled());
  return static_cast<ReloadButton&>(GetReloadControl());
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::InstrumentToolbar() {
  if (features::IsWebUIToolbarEnabled()) {
    return Steps(
        InstrumentTab(TabId()),
        InstrumentNonTabWebView(WebUIToolbarId(), GetWebUIToolbarWebView(),
                                /*wait_for_ready=*/true));
  }
  return Steps(InstrumentTab(TabId()));
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::
    WebUIAndViewsToolbarInteractiveUiTestBase::MoveMouseOverReloadButton() {
  if (features::IsWebUIReloadButtonEnabled()) {
    return Steps(MoveMouseTo(WebUIToolbarId(), WebUIReloadButtonDeepQuery()),
                 WaitForReloadHover(/*hover=*/true));
  }
  return Steps(MoveMouseTo(kReloadButtonElementId));
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::MoveMouseOffOfReloadButton() {
#if BUILDFLAG(IS_MAC)
  if (features::IsWebUIReloadButtonEnabled()) {
    return Steps(
        MoveMouseTo(WebUIToolbarId(), WebUIBackForwardButtonDeepQuery()),
        WaitForReloadHover(/*hover=*/false));
  }
#endif  // BUILDFLAG(IS_MAC)
  return Steps(MoveMouseTo(TabId()));
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::WaitForReloadButtonReady() {
  if (features::IsWebUIReloadButtonEnabled()) {
    return WaitForJsResultAt(WebUIToolbarId(), WebUIReloadButtonDeepQuery(),
                             "el => (!el.showStopIcon &&"
                             "  !el.doubleClickReloadIconTimer_.isRunning())",
                             true);
  }
  return PollUntil(base::BindRepeating(
                       [](const ReloadButton* reload_button) {
                         return reload_button->GetVisibleMode() ==
                                    ReloadButton::Mode::kReload &&
                                !reload_button->GetDoubleClickTimerIsRunning();
                       },
                       base::Unretained(&GetNonWebUIReloadButton())),
                   "Reload button ready");
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::WaitForReloadButtonStopIcon() {
  if (features::IsWebUIReloadButtonEnabled()) {
    return WaitForJsResultAt(WebUIToolbarId(), WebUIReloadButtonDeepQuery(),
                             R"(el => (el.showStopIcon && !el.isDisabled))",
                             true);
  }
  return PollUntil(base::BindRepeating(
                       [](const ReloadButton* reload_button) {
                         return reload_button->GetVisibleMode() ==
                                    ReloadButton::Mode::kStop &&
                                reload_button->GetEnabled();
                       },
                       base::Unretained(&GetNonWebUIReloadButton())),
                   "Reload button showing enabled stop icon");
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::
    WaitForReloadButtonDisabledStopIcon() {
  if (features::IsWebUIReloadButtonEnabled()) {
    return WaitForJsResultAt(WebUIToolbarId(), WebUIReloadButtonDeepQuery(),
                             R"(el => (el.showStopIcon && el.isDisabled))",
                             true);
  }
  return PollUntil(base::BindRepeating(
                       [](const ReloadButton* reload_button) {
                         return reload_button->GetVisibleMode() ==
                                    ReloadButton::Mode::kStop &&
                                !reload_button->GetEnabled();
                       },
                       base::Unretained(&GetNonWebUIReloadButton())),
                   "Reload button showing disabled stop icon");
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::ClickReloadButton(
    ui_controls::MouseButton button,
    bool release,
    int modifier_keys) {
  return Steps(MoveMouseOverReloadButton(), WaitForReloadButtonReady(),
               ClickMouse(button, release, modifier_keys));
}

ui::test::InteractiveTestApi::MultiStep
WebUIAndViewsToolbarInteractiveUiTestBase::WaitForReloadHover(bool hover) {
  CHECK(features::IsWebUIReloadButtonEnabled());
  return WaitForJsResultAt(WebUIToolbarId(), WebUIReloadButtonDeepQuery(),
                           R"(el => el.renderRoot.querySelector(
                                'cr-icon-button')?.matches(':hover'))",
                           hover);
}
