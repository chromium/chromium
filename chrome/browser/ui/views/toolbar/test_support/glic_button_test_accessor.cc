// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/test_support/glic_button_test_accessor.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button_interface.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace {

// Evaluates `body_js` inside a function where `glic` is bound to `#glic-button`
// and `btn` is bound to its inner `#button`.
content::EvalJsResult EvalOnGlicButton(content::WebContents* web_contents,
                                       std::string_view body_js) {
  return content::EvalJs(
      web_contents,
      base::StrCat(
          {"(() => {"
           "  const glic = document.querySelector('toolbar-app')"
           "                   ?.shadowRoot?.querySelector('#glic-button');"
           "  const btn = glic?.shadowRoot?.querySelector('#button');",
           body_js, "})()"}));
}

}  // namespace

GlicButtonTestAccessor::GlicButtonTestAccessor(BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(browser_);
}

GlicButtonTestAccessor::~GlicButtonTestAccessor() = default;

glic::ToolbarGlicButtonInterface* GlicButtonTestAccessor::GetControl() const {
  if (WebUIToolbarWebView* webui_toolbar = GetWebUIToolbarWebView(browser_)) {
    if (glic::ToolbarGlicButtonInterface* control =
            webui_toolbar->GetGlicControl()) {
      return control;
    }
  }
  return views::AsViewClass<glic::ToolbarGlicButton>(GetViewsButton());
}

views::LabelButton* GlicButtonTestAccessor::GetViewsButton() const {
  return glic::GlicButtonInterface::FromBrowser(browser_);
}

bool GlicButtonTestAccessor::IsVisible() const {
  return ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kGlicButtonElementId, BrowserElements::From(browser_)->GetContext());
}

bool GlicButtonTestAccessor::WaitForVisible() const {
  if (IsVisible()) {
    return true;
  }
  base::RunLoop run_loop;
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kGlicButtonElementId, BrowserElements::From(browser_)->GetContext(),
          base::IgnoreArgs<ui::TrackedElement*>(run_loop.QuitClosure()));
  run_loop.Run();
  return IsVisible();
}

bool GlicButtonTestAccessor::WaitForHidden() const {
  if (!IsVisible()) {
    return true;
  }
  base::RunLoop run_loop;
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          kGlicButtonElementId, BrowserElements::From(browser_)->GetContext(),
          base::IgnoreArgs<ui::TrackedElement*>(run_loop.QuitClosure()));
  run_loop.Run();
  return !IsVisible();
}

std::string GlicButtonTestAccessor::GetAriaLabel() const {
  if (views::LabelButton* button = GetViewsButton()) {
    return base::UTF16ToUTF8(button->GetViewAccessibility().GetCachedName());
  }
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return std::string();
  }
  return EvalOnGlicButton(web_contents, "return btn?.ariaLabel ?? '';")
      .ExtractString();
}

std::string GlicButtonTestAccessor::GetTooltip() const {
  if (views::LabelButton* button = GetViewsButton()) {
    return base::UTF16ToUTF8(button->GetTooltipText());
  }
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return std::string();
  }
  return EvalOnGlicButton(web_contents, "return btn?.tooltip ?? '';")
      .ExtractString();
}

bool GlicButtonTestAccessor::WaitForAriaLabel(std::string_view expected) const {
  return base::test::RunUntil(
      [this, expected]() { return GetAriaLabel() == expected; });
}

bool GlicButtonTestAccessor::IsMenuOpen() const {
  content::WebContents* web_contents = GetWebContents();
  return web_contents &&
         EvalOnGlicButton(web_contents,
                          "return !!btn && btn.hasAttribute('is-menu-open');")
             .ExtractBool();
}

bool GlicButtonTestAccessor::IsAriaExpanded() const {
  content::WebContents* web_contents = GetWebContents();
  return web_contents &&
         EvalOnGlicButton(web_contents,
                          "return !!btn && btn.ariaExpanded === 'true';")
             .ExtractBool();
}

bool GlicButtonTestAccessor::IsCollapsed() const {
  return HasCollapsedState(true);
}

bool GlicButtonTestAccessor::WaitForCollapsed(bool collapsed) const {
  return base::test::RunUntil(
      [this, collapsed]() { return HasCollapsedState(collapsed); });
}

bool GlicButtonTestAccessor::HasCollapsedState(bool collapsed) const {
  if (views::LabelButton* button = GetViewsButton()) {
    return button->GetText().empty() == collapsed;
  }
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return false;
  }
  return EvalOnGlicButton(
             web_contents,
             content::JsReplace(
                 "const text = glic?.shadowRoot?.querySelector('#text');"
                 "return !!glic && !!text &&"
                 "       glic.hasAttribute('collapsed') === $1 &&"
                 "       glic.hasAttribute('has-label') === !$1 &&"
                 "       (window.getComputedStyle(text).display === 'none') "
                 "=== $1;",
                 collapsed))
      .ExtractBool();
}

void GlicButtonTestAccessor::Click() const {
  if (views::LabelButton* button = GetViewsButton()) {
    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    return;
  }
  content::WebContents* web_contents = GetWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(true, EvalOnGlicButton(web_contents, "btn.click(); return true;"));
}

void GlicButtonTestAccessor::RightClick() const {
  if (views::LabelButton* button = GetViewsButton()) {
    button->ShowContextMenu(gfx::Point(), ui::mojom::MenuSourceType::kMouse);
    return;
  }
  content::WebContents* web_contents = GetWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(true,
            EvalOnGlicButton(web_contents,
                             "btn.dispatchEvent(new MouseEvent('contextmenu', "
                             "    {button: 2, bubbles: true, composed: true}));"
                             "return true;"));
}

content::WebContents* GlicButtonTestAccessor::GetWebContents() const {
  WebUIToolbarWebView* webui_toolbar = GetWebUIToolbarWebView(browser_);
  if (!webui_toolbar || !webui_toolbar->GetWebViewForTesting()) {
    return nullptr;
  }
  return webui_toolbar->GetWebViewForTesting()->GetWebContents();
}
