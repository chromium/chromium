// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_CONTROLLER_H_

#include <cstddef>
#include <memory>
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"
#include "ui/views/widget/widget_observer.h"

class PermissionPromptChipModel;
class LocationBarView;
// ButtonController that NotifyClick from being called when the
// BubbleOwnerDelegate's bubble is showing. Otherwise the bubble will show again
// immediately after being closed via losing focus.
class BubbleOwnerDelegate {
 public:
  virtual bool IsBubbleShowing() = 0;
  virtual bool IsAnimating() const = 0;
  virtual void RestartTimersOnMouseHover() = 0;
};

// This class controls a chip UI view to surface permission related information
// and prompts. For its creation, the controller expects an object of type
// OmniboxChipButton which should be a child view of another view. No ownership
// is transferred through the creation, and the controller will never destruct
// the OmniboxChipButton object. The controller and it's view are intended to
// be long-lived.
class ChipController : public permissions::PermissionRequestManager::Observer,
                       public views::WidgetObserver,
                       public BubbleOwnerDelegate {
 public:
  ChipController(Browser* browser_, OmniboxChipButton* chip_view);

  ~ChipController() override;
  ChipController(const ChipController&) = delete;
  ChipController& operator=(const ChipController&) = delete;

  // PermissionRequestManager::Observer
  void OnPermissionRequestManagerDestructed() override;

  // BubbleOwnerDelegate
  bool IsBubbleShowing() override;
  bool IsAnimating() const override;
  void RestartTimersOnMouseHover() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Displays a permission prompt using the chip UI.
  void ShowPermissionPrompt(permissions::PermissionPrompt::Delegate* delegate);

  // Chip View
  OmniboxChipButton* chip() { return chip_; }

  // Hide and clean up the entire chip
  void FinalizeChip();

  // Hide and clean up permission parts of the chip
  void FinalizePermissionPromptChip();

  // State
  bool IsPermissionPromptChipVisible() {
    return chip_ && chip_->GetVisible() && permission_prompt_model_;
  }

  // Update Browser
  void UpdateBrowser(Browser* browser) { browser_ = browser; }

  views::Widget* GetPromptBubbleWidget();

  // Testing helpers
  bool should_start_open_for_testing();
  bool should_expand_for_testing();

  bool is_collapse_timer_running_for_testing() {
    CHECK_IS_TEST();
    return collapse_timer_.IsRunning();
  }

  bool is_dismiss_timer_running_for_testing() {
    CHECK_IS_TEST();
    return dismiss_timer_.IsRunning();
  }

  void stop_animation_for_test() {
    CHECK_IS_TEST();
    chip_->animation_for_testing()->Stop();
    OnExpandAnimationEnded();
  }

  views::View* get_prompt_bubble_view_for_testing() {
    CHECK_IS_TEST();
    return prompt_bubble_tracker_.view();
  }

 private:
  // Animations
  void AnimateExpand(
      base::RepeatingCallback<void()> expand_anmiation_ended_callback);
  void AnimateCollapse() { chip_->AnimateCollapse(); }

  // Permission prompt chip functionality
  void AnnouncePermissionRequestForAccessibility(const std::u16string& text);
  void CollapseChip(bool allow_restart);

  // Permission prompt bubble functiontionality
  void OpenPermissionPromptBubble();
  void ClosePermissionPromptBubbleWithReason(
      views::Widget::ClosedReason reason);

  // Statistics
  void RecordChipButtonPressed(const char* recordKey);

  // Event handling
  void ObservePromptBubble();
  void OnPromptBubbleDismissed();
  void OnPromptExpired();
  void OnChipButtonPressed();
  void OnExpandAnimationEnded();
  void OnChipVisibilityChanged();

  // Timer functionality
  void StartCollapseTimer();
  void StartDismissTimer();
  void ResetTimers();

  // The location bar view to which the chip is attached
  LocationBarView* GetLocationBarView();

  // The chip view this controller modifies
  raw_ptr<OmniboxChipButton> chip_;

  raw_ptr<Browser> browser_;

  // The time when the chip was displayed.
  base::TimeTicks chip_shown_time_;

  // A timer used to dismiss the permission request after it's been collapsed
  // for a while.
  base::OneShotTimer dismiss_timer_;

  // A timer used to collapse the chip after a delay.
  base::OneShotTimer collapse_timer_;

  // The model of a permission prompt if one is present.
  std::unique_ptr<PermissionPromptChipModel> permission_prompt_model_;

  views::ViewTracker prompt_bubble_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_CONTROLLER_H_
