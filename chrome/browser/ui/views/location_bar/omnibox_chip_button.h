// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_

#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "chrome/browser/ui/views/permissions/permission_chip_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/md_text_button.h"

// UI component for chip button located in the omnibox. A button with an icon
// and text, with rounded corners.
class OmniboxChipButton : public views::MdTextButton {
 public:
  METADATA_HEADER(OmniboxChipButton);
  explicit OmniboxChipButton(PressedCallback callback);
  OmniboxChipButton(const OmniboxChipButton& button) = delete;
  OmniboxChipButton& operator=(const OmniboxChipButton& button) = delete;
  ~OmniboxChipButton() override;

  void AnimateCollapse();
  void AnimateExpand();
  void ResetAnimation(double value = 0);
  void SetExpandAnimationEndedCallback(
      base::RepeatingCallback<void()> callback);
  bool is_fully_collapsed() const { return fully_collapsed_; }
  bool is_animating() const { return animation_->is_animating(); }
  gfx::SlideAnimation* animation_for_testing() { return animation_.get(); }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::MdTextButton:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;
  void UpdateBackgroundColor() override;

  // Set the button theme.
  void SetTheme(OmniboxChipTheme theme);
  void SetMessage(std::u16string message);
  void SetForceExpandedForTesting(bool force_expanded_for_testing);

  void SetShowBlockedIcon(bool show_blocked_icon);

  void SetPermissionChipDelegate(
      PermissionChipDelegate* permission_chip_delegate);

  void Finalize();

  OmniboxChipTheme get_theme_for_testing() { return theme_; }

 protected:
  virtual ui::ImageModel GetIconImageModel() const;
  virtual const gfx::VectorIcon& GetIcon() const;
  // Updates the icon, and then updates text, icon, and background colors from
  // the theme.
  void UpdateIconAndColors();

 private:
  int GetIconSize() const;

  SkColor GetTextAndIconColor() const;

  SkColor GetBackgroundColor() const;

  // An animation used for expanding and collapsing the chip.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  OmniboxChipTheme theme_ = OmniboxChipTheme::kNormalVisibility;

  // If chip is collapsed. In the collapsed state, only an icon is visible,
  // without text.
  bool fully_collapsed_ = false;

  bool show_blocked_icon_ = false;

  base::RepeatingCallback<void()> expand_animation_ended_callback_;

  bool force_expanded_for_testing_ = false;

  absl::optional<PermissionChipDelegate*> permission_chip_delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_
