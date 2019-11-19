// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_

#include "ui/views/controls/button/md_text_button.h"

class OmniboxPopupContentsView;
class OmniboxResultView;

class OmniboxTabSwitchButton : public views::MdTextButton {
 public:
  OmniboxTabSwitchButton(OmniboxPopupContentsView* popup_contents_view,
                         OmniboxResultView* result_view,
                         const base::string16& hint,
                         const base::string16& hint_short,
                         const gfx::VectorIcon& icon,
                         const ui::ThemeProvider* theme_provider);

  ~OmniboxTabSwitchButton() override;

  // views::MdTextButton:
  void StateChanged(ButtonState old_state) override;

  // Called by parent views to change background on external (not mouse related)
  // event (tab key).
  void UpdateBackground();

  // Called by parent view to provide the width of the surrounding area
  // so the button can adjust its size or even presence.
  void ProvideWidthHint(int width);

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
  int CalculateGoalWidth(int parent_width, base::string16* goal_text);

  // The ancestor views.
  OmniboxPopupContentsView* const popup_contents_view_;
  OmniboxResultView* const result_view_;

  // Only calculate the width of various contents once.
  static bool calculated_widths_;
  static int icon_only_width_;
  static int short_text_width_;
  static int full_text_width_;

  // Label strings for hint text and its short version (may be same).
  base::string16 hint_;
  base::string16 hint_short_;

  const ui::ThemeProvider* theme_provider_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxTabSwitchButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
