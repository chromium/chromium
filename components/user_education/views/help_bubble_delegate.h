// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_DELEGATE_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_DELEGATE_H_

#include <vector>

#include "ui/base/accelerators/accelerator.h"
#include "ui/color/color_id.h"

namespace ui {
class TrackedElement;
}

namespace user_education {

// Provides access to app-specific strings, styles, and navigation accelerators
// so we can properly handle them.
class HelpBubbleDelegate {
 public:
  HelpBubbleDelegate() = default;
  HelpBubbleDelegate(const HelpBubbleDelegate&) = delete;
  void operator=(const HelpBubbleDelegate&) = delete;
  virtual ~HelpBubbleDelegate() = default;

  // Gets a list of accelerators that can be used to navigate panes, which
  // should trigger HelpBubble::ToggleFocusForAccessibility(). We need this
  // because we do not by default have access to the current app's
  // accelerator provider nor to the specific command IDs.
  virtual std::vector<ui::Accelerator> GetPaneNavigationAccelerators(
      ui::TrackedElement* anchor_element) const = 0;

  // These methods return text contexts that will be handled by the app's
  // typography system.
  virtual int GetTitleTextContext() const = 0;
  virtual int GetBodyTextContext() const = 0;

  // These methods return color codes that will be handled by the app's theming
  // system.
  virtual ui::ColorId GetHelpBubbleBackgroundColorId() const = 0;
  virtual ui::ColorId GetHelpBubbleForegroundColorId() const = 0;
  virtual ui::ColorId GetHelpBubbleDefaultButtonBackgroundColorId() const = 0;
  virtual ui::ColorId GetHelpBubbleDefaultButtonForegroundColorId() const = 0;
  virtual ui::ColorId GetHelpBubbleButtonBorderColorId() const = 0;
  virtual ui::ColorId GetHelpBubbleCloseButtonInkDropColorId() const = 0;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_DELEGATE_H_
