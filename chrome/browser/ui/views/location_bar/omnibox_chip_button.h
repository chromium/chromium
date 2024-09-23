// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_

#include "base/check_is_test.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/md_text_button.h"

// UI component for chip button located in the omnibox. A button with an icon
// and text, with rounded corners.
class OmniboxChipButton : public views::MdTextButton {
  METADATA_HEADER(OmniboxChipButton, views::MdTextButton)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChipElementId);
  explicit OmniboxChipButton(PressedCallback callback);
  OmniboxChipButton(const OmniboxChipButton& button) = delete;
  OmniboxChipButton& operator=(const OmniboxChipButton& button) = delete;
  ~OmniboxChipButton() override;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnChipVisibilityChanged(bool is_visible) {}
  };

  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  void AnimateCollapse(base::TimeDelta duration);
  void AnimateExpand(base::TimeDelta duration);
  void ResetAnimation(double value = 0);
  bool is_fully_collapsed() const { return fully_collapsed_; }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::MdTextButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;
  void UpdateBackgroundColor() override;

  // Customize the button.
  void SetTheme(OmniboxChipTheme theme);
  void SetMessage(std::u16string message);

  OmniboxChipTheme GetOmniboxChipTheme() const { return theme_; }

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // The following virtual functions are used for the non-error state permission
  // chips (default/neutral states). For any other changes to the look and feel
  // of the chips, consider subclassing and overriding as needed.
  virtual ui::ImageModel GetIconImageModel() const;
  virtual const gfx::VectorIcon& GetIcon() const;
  virtual ui::ColorId GetForegroundColorId() const;
  virtual ui::ColorId GetBackgroundColorId() const;

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

  // An animation used for expanding and collapsing the chip.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  OmniboxChipTheme theme_ = OmniboxChipTheme::kLowVisibility;

  // If chip is collapsed. In the collapsed state, only an icon is visible,
  // without text.
  bool fully_collapsed_ = false;

  raw_ptr<const gfx::VectorIcon> icon_ = &gfx::kNoneIcon;

  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_BUTTON_H_
