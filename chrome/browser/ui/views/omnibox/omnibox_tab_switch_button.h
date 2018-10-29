// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_

#include "ui/views/controls/button/md_text_button.h"

class OmniboxPopupContentsView;
class OmniboxResultView;

namespace gfx {
class SlideAnimation;
}

class OmniboxTabSwitchButton : public views::MdTextButton {
 public:
  OmniboxTabSwitchButton(OmniboxPopupContentsView* model,
                         OmniboxResultView* result_view,
                         const base::string16& hint,
                         const base::string16& hint_short,
                         const gfx::VectorIcon& icon);

  ~OmniboxTabSwitchButton() override;

  // views::MdTextButton:
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void StateChanged(ButtonState old_state) override;

  // Called by parent views to change background on external (not mouse related)
  // event (tab key).
  void UpdateBackground();

  // Called by parent view to provide the width of the surrounding area
  // so the button can adjust its size or even presence.
  void ProvideWidthHint(size_t width);

  // Called to indicate button has been focused.
  void ProvideFocusHint();
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // Consults the parent views to see if the button is selected.
  bool IsSelected() const;

  // Produces a path custom to this button for the focus ring to follow.
  SkPath GetFocusRingPath() const;

  // Encapsulates the color look-up, which uses the button state (hovered,
  // etc.) and consults the parent result view.
  SkColor GetBackgroundColor() const;

  // Encapsulates changing the color of the button to display being
  // pressed.
  void SetPressed();

  // Helper function to translate parent width into goal width, and
  // pass back the text at that width.
  size_t CalculateGoalWidth(size_t parent_width, base::string16* goal_text);

  static constexpr int kButtonHeight = 32;
  OmniboxPopupContentsView* model_;
  OmniboxResultView* result_view_;

  // Only calculate the width of various contents once.
  static bool calculated_widths_;
  static size_t icon_only_width_;
  static size_t short_text_width_;
  static size_t full_text_width_;

  // To distinguish start-up case, where we don't want animation.
  bool initialized_;
  // Animation starting width, and final value.
  size_t start_width_, goal_width_;
  // The text to be displayed when we reach |goal_width_|.
  base::string16 goal_text_;
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Label strings for hint text and its short version (may be same).
  base::string16 hint_;
  base::string16 hint_short_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxTabSwitchButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
