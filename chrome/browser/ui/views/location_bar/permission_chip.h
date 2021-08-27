// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_observer.h"

class BubbleOwnerDelegate {
 public:
  virtual bool IsBubbleShowing() const = 0;
};

// A class for an interface for chip view that is shown in the location bar to
// notify user about a permission request.
class PermissionChip : public views::AccessiblePaneView,
                       public views::WidgetObserver,
                       public BubbleOwnerDelegate {
 protected:
  // Holds all parameters needed for a chip initialization.
  struct DisplayParams {
    const gfx::VectorIcon& icon;
    std::u16string message;
    bool should_start_open;
    bool is_prominent;
    OmniboxChipButton::Theme theme;
    bool should_expand;
  };

 public:
  METADATA_HEADER(PermissionChip);
  explicit PermissionChip(permissions::PermissionPrompt::Delegate* delegate,
                          DisplayParams initializer);
  PermissionChip(const PermissionChip& chip) = delete;
  PermissionChip& operator=(const PermissionChip& chip) = delete;
  ~PermissionChip() override;

  // Opens the permission prompt bubble.
  void OpenBubble();

  void Hide();
  void Reshow();

  views::Button* button() { return chip_button_; }
  bool is_fully_collapsed() const { return chip_button_->is_fully_collapsed(); }

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void AddedToWidget() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // BubbleOwnerDelegate:
  bool IsBubbleShowing() const override;

  views::Widget* GetPromptBubbleWidgetForTesting();

  bool should_start_open_for_testing() { return should_start_open_; }
  bool should_expand_for_testing() { return should_expand_; }
  OmniboxChipButton* get_chip_button_for_testing() { return chip_button_; }

 protected:
  // Returns a newly-created permission prompt bubble.
  virtual views::View* CreateBubble() WARN_UNUSED_RESULT = 0;

  permissions::PermissionPrompt::Delegate* delegate() const {
    return delegate_;
  }

  views::Widget* GetPromptBubbleWidget();

  virtual void Collapse(bool allow_restart);
  void ShowBlockedBadge();

 private:
  void Show(bool always_open_bubble);
  void ExpandAnimationEnded();
  void ChipButtonPressed();
  void RestartTimersOnInteraction();
  void StartCollapseTimer();
  void StartDismissTimer();
  void Dismiss();

  void AnimateCollapse();
  void AnimateExpand();

  permissions::PermissionPrompt::Delegate* const delegate_;

  // ViewTracker used to track the prompt bubble.
  views::ViewTracker prompt_bubble_tracker_;

  // A timer used to collapse the chip after a delay.
  base::OneShotTimer collapse_timer_;

  // A timer used to dismiss the permission request after it's been collapsed
  // for a while.
  base::OneShotTimer dismiss_timer_;

  // The button that displays the icon and text.
  OmniboxChipButton* chip_button_ = nullptr;

  bool should_start_open_ = false;
  bool should_expand_ = true;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_
