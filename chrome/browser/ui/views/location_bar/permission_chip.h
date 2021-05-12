// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class PermissionPromptBubbleView;

namespace views {
class Widget;
}  // namespace views

class BubbleOwnerDelegate {
 public:
  virtual bool IsBubbleShowing() const = 0;
};

// A chip view shown in the location bar to notify user about a permission
// request. Shows a permission bubble on click.
class PermissionChip : public views::AccessiblePaneView,
                       public views::WidgetObserver,
                       public BubbleOwnerDelegate {
 public:
  METADATA_HEADER(PermissionChip);
  explicit PermissionChip(Browser* browser);
  PermissionChip(const PermissionChip& chip) = delete;
  PermissionChip& operator=(const PermissionChip& chip) = delete;
  ~PermissionChip() override;

  void DisplayRequest(permissions::PermissionPrompt::Delegate* delegate);
  void FinalizeRequest();
  void OpenBubble();
  void Hide();
  void Reshow();
  bool GetActiveRequest() const;

  views::Button* button() { return chip_button_; }
  bool is_fully_collapsed() const { return chip_button_->is_fully_collapsed(); }

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // BubbleOwnerDelegate:
  bool IsBubbleShowing() const override;

  PermissionPromptBubbleView* prompt_bubble_for_testing() {
    return prompt_bubble_;
  }

 private:
  bool ShouldBubbleStartOpen() const;

  void Show(bool always_open_bubble);
  void ExpandAnimationEnded();
  void ChipButtonPressed();
  void RestartTimersOnInteraction();
  void StartCollapseTimer();
  void Collapse(bool allow_restart);
  void StartDismissTimer();
  void Dismiss();
  std::u16string GetPermissionMessage() const;
  const gfx::VectorIcon& GetPermissionIconId() const;

  void AnimateCollapse();
  void AnimateExpand();

  Browser* browser_ = nullptr;
  permissions::PermissionPrompt::Delegate* delegate_ = nullptr;
  PermissionPromptBubbleView* prompt_bubble_ = nullptr;

  // A timer used to collapse the chip after a delay.
  base::OneShotTimer collapse_timer_;

  // A timer used to dismiss the permission request after it's been collapsed
  // for a while.
  base::OneShotTimer dismiss_timer_;

  // The button that displays the icon and text.
  OmniboxChipButton* chip_button_ = nullptr;

  // The time when the permission was requested.
  base::TimeTicks requested_time_;

  // If uma metric was already recorded on the button click.
  bool already_recorded_interaction_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_
