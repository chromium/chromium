// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_CHIP_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/browser/ui/views/permissions/permission_chip_delegate.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_observer.h"

class BubbleOwnerDelegate {
 public:
  virtual bool IsBubbleShowing() const = 0;
  virtual bool IsAnimating() const = 0;
  virtual void RestartTimersOnMouseHover() = 0;
};

// A class for an interface for chip view that is shown in the location bar to
// notify user about a permission request.
class PermissionChip : public views::AccessiblePaneView,
                       public views::WidgetObserver,
                       public BubbleOwnerDelegate {
 public:
  METADATA_HEADER(PermissionChip);
  PermissionChip();
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
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // BubbleOwnerDelegate:
  bool IsBubbleShowing() const override;
  bool IsAnimating() const override;
  void RestartTimersOnMouseHover() override;

  void SetupChip(
      std::unique_ptr<PermissionChipDelegate> permission_chip_delegate);

  void Finalize();
  bool IsInitialized();
  // The chip is fully initialized and visible.
  bool IsActive();

  views::Widget* GetPromptBubbleWidgetForTesting();

  views::View* get_prompt_bubble_view_for_testing() {
    return prompt_bubble_tracker_.view();
  }

  bool should_start_open_for_testing() { return should_start_open_; }
  bool should_expand_for_testing() { return should_expand_; }
  OmniboxChipButton* get_chip_button_for_testing() { return chip_button_; }

  void stop_animation_for_test() {
    chip_button_->animation_for_testing()->Stop();
    ExpandAnimationEnded();
  }

  bool is_collapse_timer_running_for_testing() {
    return collapse_timer_.IsRunning();
  }

  bool is_dismiss_timer_running_for_testing() {
    return dismiss_timer_.IsRunning();
  }

 protected:
  permissions::PermissionPrompt::Delegate* delegate() const {
    return permission_prompt_delegate_.value();
  }

  views::Widget* GetPromptBubbleWidget();

  void ShowBlockedIcon();

 private:
  void Show(bool always_open_bubble);
  void ExpandAnimationEnded();
  void ChipButtonPressed();
  void StartCollapseTimer();
  void StartDismissTimer();
  void Collapse(bool allow_restart);

  void ResetTimers() {
    collapse_timer_.AbandonAndStop();
    dismiss_timer_.AbandonAndStop();
  }

  void AnnounceChip();
  void OnPromptBubbleDismissed();

  absl::optional<permissions::PermissionPrompt::Delegate*>
      permission_prompt_delegate_;

  std::unique_ptr<PermissionChipDelegate> permission_chip_delegate_;

  // ViewTracker used to track the prompt bubble.
  views::ViewTracker prompt_bubble_tracker_;

  // A timer used to collapse the chip after a delay.
  base::OneShotTimer collapse_timer_;

  // A timer used to dismiss the permission request after it's been collapsed
  // for a while.
  base::OneShotTimer dismiss_timer_;

  // The button that displays the icon and text.
  raw_ptr<OmniboxChipButton> chip_button_ = nullptr;

  bool should_start_open_ = false;
  bool should_expand_ = true;
  bool should_dismiss_ = false;

  base::WeakPtrFactory<PermissionChip> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_CHIP_H_
