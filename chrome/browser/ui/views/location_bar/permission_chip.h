// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_

#include "base/timer/timer.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/button/md_text_button.h"
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
class PermissionChip : public views::View,
                       public views::AnimationDelegateViews,
                       public views::WidgetObserver,
                       public BubbleOwnerDelegate {
 public:
  explicit PermissionChip(Browser* browser);
  PermissionChip(const PermissionChip& mask_layer) = delete;
  PermissionChip& operator=(const PermissionChip& mask_layer) = delete;

  ~PermissionChip() override;

  void Show(permissions::PermissionPrompt::Delegate* delegate);
  void Hide();

  views::Button* button() { return chip_button_; }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // BubbleOwnerDelegate:
  bool IsBubbleShowing() const override;

 private:
  void ChipButtonPressed();
  void Collapse();
  void StartCollapseTimer();
  int GetIconSize() const;
  void UpdatePermissionIconAndTextColor();
  base::string16 GetPermissionMessage();
  const gfx::VectorIcon& GetPermissionIconId();

  Browser* browser_ = nullptr;
  permissions::PermissionPrompt::Delegate* delegate_ = nullptr;
  PermissionPromptBubbleView* prompt_bubble_ = nullptr;

  // An animation used for expanding and collapsing the chip.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // A timer used to collapse the chip after a delay.
  base::OneShotTimer timer_;

  // The button that displays the icon and text.
  views::MdTextButton* chip_button_;

  // The time when the permission was requested.
  base::TimeTicks requested_time_;

  // If uma metric was already recorded on the button click.
  bool already_recorded_interaction_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_
