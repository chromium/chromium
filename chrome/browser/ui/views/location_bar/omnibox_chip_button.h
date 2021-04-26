// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_

#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

// UI component for chip button located in the omnibox. A button with an icon
// and text, with rounded corners.
class OmniboxChipButton : public views::MdTextButton {
 public:
  METADATA_HEADER(OmniboxChipButton);
  explicit OmniboxChipButton(
      PressedCallback callback,
      int button_context = views::style::CONTEXT_BUTTON_MD);
  OmniboxChipButton(const OmniboxChipButton& button) = delete;
  OmniboxChipButton& operator=(const OmniboxChipButton& button) = delete;
  ~OmniboxChipButton() override;

  // Icon, text, and background colors that should be used for different types
  // of Chip.
  enum class Theme {
    kBlue,
    // TODO(crbug.com/1177760): Other themes will follow.
  };

  void AnimateCollapse();
  void AnimateExpand();
  void ResetAnimation(double value = 0);

  void SetIcon(const gfx::VectorIcon* icon);
  void SetExpandAnimationEndedCallback(
      base::RepeatingCallback<void()> callback);

  bool is_fully_collapsed() const { return fully_collapsed_; }
  bool is_animating() const { return animation_->is_animating(); }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::MdTextButton:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  // Set the button theme.
  void SetTheme(Theme theme);

  // Set whether the button uses prominent styling, equivalent to
  // MdTextButton::SetProminent.
  void SetProminent(bool is_prominent);

  void SetForceExpandedForTesting(bool force_expanded_for_testing);

 private:
  int GetIconSize() const;

  // Apply colors to text, icon and background of the button.
  void UpdateColors();

  // Returns the primary theme color.
  SkColor GetMainColor();

  // Returns the color that is used for the prominent button's text and icon, or
  // the non-prominent button's background. The return color matches the toolbar
  // color.
  SkColor GetNeutralColor();

  // Get the color for the text and icon.
  SkColor GetForegroundColor();

  // If button is prominent, the background will be filled in theme color,
  // otherwise the background will have the neutral color with a theme-colored
  // border stroke.
  SkColor GetBackgroundColor();

  // An animation used for expanding and collapsing the chip.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  Theme theme_ = Theme::kBlue;

  // If chip is collapsed. In the collapsed state, only an icon is visible,
  // without text.
  bool fully_collapsed_ = false;

  const gfx::VectorIcon* icon_ = nullptr;

  base::RepeatingCallback<void()> expand_animation_ended_callback_;

  bool force_expanded_for_testing_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_
