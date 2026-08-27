// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_test_accessor.h"

#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "ui/events/test/test_event.h"

LocationIconTestAccessor::LocationIconTestAccessor(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

LocationIconTestAccessor::~LocationIconTestAccessor() = default;

content::WebContents* LocationIconTestAccessor::GetWebContents() {
  if (!browser_) {
    return nullptr;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return nullptr;
  }
  WebUIToolbarWebView* webui_view =
      browser_view->toolbar_button_provider()->GetWebUIToolbarViewForTesting();
  if (!webui_view || !webui_view->GetWebViewForTesting()) {
    return nullptr;
  }
  return webui_view->GetWebViewForTesting()->web_contents();
}

LocationIconView* LocationIconTestAccessor::GetLocationIconView() {
  if (!browser_) {
    return nullptr;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view) {
    return nullptr;
  }
  if (auto* location_bar_view = browser_view->GetLocationBarView()) {
    return location_bar_view->location_icon_view();
  }
  return nullptr;
}

bool LocationIconTestAccessor::IsBubbleShowing() const {
  return PageInfoBubbleViewBase::GetShownBubbleType() !=
         PageInfoBubbleViewBase::BUBBLE_NONE;
}

void LocationIconTestAccessor::Click() {
  if (!browser_) {
    return;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view) {
    return;
  }

  // Check for views path first; it can be used with fancy windows like
  // webapps even if WebUILocationBar is on for regular windows.
  if (auto* location_icon_view = GetLocationIconView()) {
    ui::test::TestEvent event;
    location_icon_view->ShowBubble(event);
    return;
  }

  if (features::IsWebUILocationBarEnabled()) {
    if (!browser_view->toolbar_button_provider()) {
      return;
    }

    WebUIToolbarWebView* webui_view = browser_view->toolbar_button_provider()
                                          ->GetWebUIToolbarViewForTesting();
    if (!webui_view) {
      return;
    }

    webui_view->OnLhsChipClicked(
        toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon,
        /*is_mouse_interaction=*/false);
    return;
  }
}

bool LocationIconTestAccessor::ShowBubble() {
  Click();
  return IsBubbleShowing();
}

void LeftClickLocationIcon(BrowserWindowInterface* browser) {
  LocationIconTestAccessor(browser).Click();
}
