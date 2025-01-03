// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_VIEW_H_

#include "base/check_is_test.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/permissions/permission_actions_history.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view_tracker.h"

class MultiImageContainer;

// UI component for chip button located in the omnibox. A button with an icon
// and text, with rounded corners.
class PermissionChipView : public views::MdTextButton {
  METADATA_HEADER(PermissionChipView, views::MdTextButton)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kElementIdForTesting);
  explicit PermissionChipView(PressedCallback callback);
  PermissionChipView(const PermissionChipView& button) = delete;
  PermissionChipView& operator=(const PermissionChipView& button) = delete;
  ~PermissionChipView() override;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnChipVisibilityChanged(bool is_visible) {}
    virtual void OnExpandAnimationEnded() {}
    virtual void OnCollapseAnimationEnded() {}
    virtual void OnMousePressed() {}
  };

  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  void AnimateCollapse(base::TimeDelta duration);
  void AnimateExpand(base::TimeDelta duration);
  void AnimateToFit(base::TimeDelta duration);
  void ResetAnimation(double value = 0);
  bool is_fully_collapsed() const { return fully_collapsed_; }
  bool is_animating() const { return animation_->is_animating(); }
  gfx::SlideAnimation* animation_for_testing() { return animation_.get(); }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::MdTextButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;
  void UpdateBackgroundColor() override;

  // Customize the button.
  void SetUserDecision(permissions::PermissionAction user_decision);
  void SetTheme(PermissionChipTheme theme);
  void SetMessage(std::u16string message);
  void SetBlockedIconShowing(bool should_show_blocked_icon);
  void SetPermissionPromptStyle(PermissionPromptStyle prompt_style);
  void SetChipIcon(const gfx::VectorIcon& icon);
  void SetChipIcon(const gfx::VectorIcon* icon);

  bool ShouldShowBlockedIcon() const { return should_show_blocked_icon_; }
  permissions::PermissionAction GetUserDecision() const {
    return user_decision_;
  }
  PermissionPromptStyle GetPermissionPromptStyle() const {
    return prompt_style_;
  }
  PermissionChipTheme theme() const { return theme_; }

  // Returns whether the theme describes a request state (true) or indicator
  // state (false).
  bool GetIsRequestForTesting() const;

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void UpdateForDividerVisibility(bool is_divider_visible,
                                  int divider_arc_width = 0);
  int GetIconViewWidth() const;

 protected:
  MultiImageContainer* multi_image_container();

  // The following virtual functions are used for the non-error state permission
  // chips (default/neutral states). For any other changes to the look and feel
  // of the chips, consider subclassing and overriding as needed.
  virtual ui::ImageModel GetIconImageModel() const;
  virtual const gfx::VectorIcon& GetIcon() const;
  virtual SkColor GetForegroundColor() const;
  virtual SkColor GetBackgroundColor() const;

  // Updates the icon, and then updates text, icon, and background colors from
  // the theme.
  void UpdateIconAndColors();

 private:
  // Performs a full animation from 0 to 1, ending up at the preferred size of
  // the chip.
  void ForceAnimateExpand();

  // Performs a full collapse from 1 to 0, ending up at base_width_ + fixed
  // width.
  void ForceAnimateCollapse();

  void OnAnimationValueMaybeChanged();

  int GetIconSize() const;

  int GetCornerRadius() const;
  gfx::RoundedCornersF GetCornerRadii() const;

  gfx::Insets GetPadding() const;

  // An animation used for expanding and collapsing the chip.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  PermissionChipTheme theme_ = PermissionChipTheme::kNormalVisibility;

  // Denotes the chips current prompt style. This influences what appears in the
  // location bar. Currently this will be some combination of text, icon, and
  // prompt bubble.
  PermissionPromptStyle prompt_style_ = PermissionPromptStyle::kChip;

  // Denotes the current action / settings the user has selected on the omnibox
  // chip prompt. Ex: "Allow", "Allow Once", "Not allowed".
  permissions::PermissionAction user_decision_ =
      permissions::PermissionAction::GRANTED;

  // True when the blocked icon is currently showing in the omnibox chip. This
  // usually happens when the user has disabled / "Not allowed" a permission
  // such as location or notifications. False otherwise.
  bool should_show_blocked_icon_ = false;

  int base_width_ = 0;

  // If chip is collapsed. In the collapsed state, only an icon is visible,
  // without text.
  bool fully_collapsed_ = false;
  bool is_divider_visible_ = false;

  raw_ptr<const gfx::VectorIcon> icon_ = &gfx::kNoneIcon;

  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_VIEW_H_
