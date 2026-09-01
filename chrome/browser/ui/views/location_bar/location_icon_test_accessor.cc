// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_test_accessor.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/test/test_event.h"

namespace {

content::EvalJsResult EvaluateWebUI(content::WebContents* contents,
                                    std::string_view element_predicate_js) {
  if (!contents) {
    return content::EvalJsResult(base::Value(), "no web contents");
  }

  const char kScriptTemplate[] = R"(
    (function() {
      const el = document.querySelector('toolbar-app')?.shadowRoot?.
                     querySelector('location-bar')?.shadowRoot?.
                     querySelector('location-icon');
      if (!el) {
        throw "no element";
      }
      return (%s)(el);
    })();
  )";
  return content::EvalJs(
      contents, base::StringPrintf(kScriptTemplate, element_predicate_js));
}

}  // namespace

LocationIconTestAccessor::LocationIconTestAccessor(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

LocationIconTestAccessor::~LocationIconTestAccessor() = default;

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

// Returns true if the icon is visible.
bool LocationIconTestAccessor::IsVisible() {
  if (!browser_) {
    return false;
  }
  auto* browser_elements = BrowserElements::From(browser_);
  if (!browser_elements) {
    return false;
  }

  return browser_elements->GetElement(kLocationIconElementId);
}

bool LocationIconTestAccessor::IsShowingText() {
  if (auto* view = GetLocationIconView()) {
    return view->GetShowText();
  }
  auto result = EvaluateWebUI(GetWebContents(), "(el) => el.hasText");
  return result.is_bool() && result.ExtractBool();
}

std::u16string LocationIconTestAccessor::GetText() {
  if (auto* view = GetLocationIconView()) {
    return view->GetText();
  }
  // This isn't using .displayText since that can get latched with old
  // values for animation reasons.
  auto result = EvaluateWebUI(GetWebContents(), "(el) => el.state.text");
  if (result.is_string()) {
    return base::UTF8ToUTF16(result.ExtractString());
  }
  return u"";
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

bool LocationIconTestAccessor::ShowBubble() {
  Click();
  return IsBubbleShowing();
}
