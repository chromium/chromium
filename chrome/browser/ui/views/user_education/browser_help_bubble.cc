// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

BrowserHelpBubbleDelegate::BrowserHelpBubbleDelegate() = default;
BrowserHelpBubbleDelegate::~BrowserHelpBubbleDelegate() = default;

std::vector<ui::Accelerator>
BrowserHelpBubbleDelegate::GetPaneNavigationAccelerators(
    ui::TrackedElement* anchor_element) const {
  std::vector<ui::Accelerator> result;
  if (anchor_element->IsA<views::TrackedElementViews>()) {
    auto* widget =
        anchor_element->AsA<views::TrackedElementViews>()->view()->GetWidget();
    if (widget) {
      auto* const client_view = widget->GetPrimaryWindowWidget()->client_view();
      if (client_view && views::IsViewClass<BrowserView>(client_view)) {
        auto* const browser_view = static_cast<BrowserView*>(client_view);
        ui::Accelerator accel;
        if (browser_view->GetAccelerator(IDC_FOCUS_NEXT_PANE, &accel)) {
          result.push_back(accel);
        }
        if (browser_view->GetAccelerator(IDC_FOCUS_PREVIOUS_PANE, &accel)) {
          result.push_back(accel);
        }
        if (browser_view->GetAccelerator(
                IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY, &accel)) {
          result.push_back(accel);
        }
      }
    }
  }
  return result;
}

int BrowserHelpBubbleDelegate::GetTitleTextContext() const {
  return ChromeTextContext::CONTEXT_IPH_BUBBLE_TITLE;
}
int BrowserHelpBubbleDelegate::GetBodyTextContext() const {
  return ChromeTextContext::CONTEXT_IPH_BUBBLE_BODY;
}

// These methods return color codes that will be handled by the app's theming
// system.
ui::ColorId BrowserHelpBubbleDelegate::GetHelpBubbleBackgroundColorId() const {
  return kColorFeaturePromoBubbleBackground;
}
ui::ColorId BrowserHelpBubbleDelegate::GetHelpBubbleForegroundColorId() const {
  return kColorFeaturePromoBubbleForeground;
}
ui::ColorId
BrowserHelpBubbleDelegate::GetHelpBubbleDefaultButtonBackgroundColorId() const {
  return kColorFeaturePromoBubbleDefaultButtonBackground;
}
ui::ColorId
BrowserHelpBubbleDelegate::GetHelpBubbleDefaultButtonForegroundColorId() const {
  return kColorFeaturePromoBubbleDefaultButtonForeground;
}
ui::ColorId BrowserHelpBubbleDelegate::GetHelpBubbleButtonBorderColorId()
    const {
  return kColorFeaturePromoBubbleButtonBorder;
}
ui::ColorId BrowserHelpBubbleDelegate::GetHelpBubbleCloseButtonInkDropColorId()
    const {
  return kColorFeaturePromoBubbleCloseButtonInkDrop;
}

TabWebUIHelpBubbleFactoryBrowser::TabWebUIHelpBubbleFactoryBrowser() = default;
TabWebUIHelpBubbleFactoryBrowser::~TabWebUIHelpBubbleFactoryBrowser() = default;

std::unique_ptr<user_education::HelpBubble>
TabWebUIHelpBubbleFactoryBrowser::CreateBubble(
    ui::TrackedElement* element,
    user_education::HelpBubbleParams params) {
  const bool focus =
      params.focus_on_show_hint.value_or(!params.buttons.empty());
  auto result =
      HelpBubbleFactoryWebUI::CreateBubble(element, std::move(params));

  // Some bubbles should start focused.
  if (result && focus) {
    // Assuming the help bubble is in the active web contents in a browser
    // window, in order to be consistent with other help bubbles, we should
    // ensure the contents pane is focused.
    if (const auto* const contents =
            result->AsA<user_education::HelpBubbleWebUI>()->GetWebContents()) {
      if (const auto* browser = chrome::FindBrowserWithTab(contents)) {
        if (browser->tab_strip_model()->GetActiveWebContents() == contents) {
          BrowserView::GetBrowserViewForBrowser(browser)
              ->FocusWebContentsPane();
        }
      }
    }
  }

  return result;
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TabWebUIHelpBubbleFactoryBrowser)

FloatingWebUIHelpBubbleFactoryBrowser::FloatingWebUIHelpBubbleFactoryBrowser(
    const user_education::HelpBubbleDelegate* delegate)
    : FloatingWebUIHelpBubbleFactory(delegate) {}
FloatingWebUIHelpBubbleFactoryBrowser::
    ~FloatingWebUIHelpBubbleFactoryBrowser() = default;

bool FloatingWebUIHelpBubbleFactoryBrowser::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  if (!element->IsA<user_education::TrackedElementWebUI>()) {
    return false;
  }

  // If this is a WebUI in a tab, then don't use this factory.
  const auto* contents = element->AsA<user_education::TrackedElementWebUI>()
                             ->handler()
                             ->GetWebContents();
  // Note: this checks all tabs for their WebContents.
  if (chrome::FindBrowserWithTab(contents)) {
    return false;
  }

  // Ensure that this WebUI fulfils the requirements for a floating help
  // bubble.
  return FloatingWebUIHelpBubbleFactory::CanBuildBubbleForTrackedElement(
      element);
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(FloatingWebUIHelpBubbleFactoryBrowser)
