// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_HELP_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_HELP_BUBBLE_H_

#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/webui/floating_webui_help_bubble_factory.h"
#include "components/user_education/webui/help_bubble_webui.h"

// This file provides support classes required to create browser-specific help
// bubbles.

// Implementation of the help bubble delegate for chromium; uses browser theme,
// color, and accelerator mappings.
class BrowserHelpBubbleDelegate : public user_education::HelpBubbleDelegate {
 public:
  BrowserHelpBubbleDelegate();
  ~BrowserHelpBubbleDelegate() override;

  // user_education::HelpBubbleDelegate:
  std::vector<ui::Accelerator> GetPaneNavigationAccelerators(
      ui::TrackedElement* anchor_element) const override;
  int GetTitleTextContext() const override;
  int GetBodyTextContext() const override;
  ui::ColorId GetHelpBubbleBackgroundColorId() const override;
  ui::ColorId GetHelpBubbleForegroundColorId() const override;
  ui::ColorId GetHelpBubbleDefaultButtonBackgroundColorId() const override;
  ui::ColorId GetHelpBubbleDefaultButtonForegroundColorId() const override;
  ui::ColorId GetHelpBubbleButtonBorderColorId() const override;
  ui::ColorId GetHelpBubbleCloseButtonInkDropColorId() const override;
};

// Help bubble factory that can show an embedded (WebUI-based) help bubble on a
// tab in the browser. This takes the added step of conditionally focusing the
// contents pane of the browser if the help bubble is in the active tab.
class TabWebUIHelpBubbleFactoryBrowser
    : public user_education::HelpBubbleFactoryWebUI {
 public:
  TabWebUIHelpBubbleFactoryBrowser();
  ~TabWebUIHelpBubbleFactoryBrowser() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // user_education::HelpBubbleFactoryWebUI:
  std::unique_ptr<user_education::HelpBubble> CreateBubble(
      ui::TrackedElement* element,
      user_education::HelpBubbleParams params) override;
};

// Help bubble factory that can show a floating (Views-based) help bubble on a
// WebUI element, but only for non-tab WebUI.
class FloatingWebUIHelpBubbleFactoryBrowser
    : public user_education::FloatingWebUIHelpBubbleFactory {
 public:
  explicit FloatingWebUIHelpBubbleFactoryBrowser(
      const user_education::HelpBubbleDelegate* delegate);
  ~FloatingWebUIHelpBubbleFactoryBrowser() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubbleFactoryWebUIViews:
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_HELP_BUBBLE_H_
