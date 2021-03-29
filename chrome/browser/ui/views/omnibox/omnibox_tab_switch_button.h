// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_

#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_header_macros.h"

class OmniboxPopupContentsView;
class OmniboxResultView;

class OmniboxTabSwitchButton : public views::MdTextButton {
 public:
  METADATA_HEADER(OmniboxTabSwitchButton);

  OmniboxTabSwitchButton(PressedCallback callback,
                         OmniboxPopupContentsView* popup_contents_view,
                         OmniboxResultView* result_view,
                         const std::u16string& hint,
                         const std::u16string& hint_short,
                         const gfx::VectorIcon& icon);
  OmniboxTabSwitchButton(const OmniboxTabSwitchButton&) = delete;
  OmniboxTabSwitchButton& operator=(const OmniboxTabSwitchButton&) = delete;
  ~OmniboxTabSwitchButton() override;

  // views::MdTextButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void StateChanged(ButtonState old_state) override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Returns the FlexRule that should be used to size this button.
  static views::FlexRule GetFlexRule();

  // Called by parent views to change background on external (not mouse related)
  // event (tab key).
  void UpdateBackground();

 private:
  // Consults the parent views to see if the button is selected.
  bool IsSelected() const;

  // The ancestor views.
  OmniboxPopupContentsView* const popup_contents_view_;
  OmniboxResultView* const result_view_;

  // Only calculate the width of various contents once.
  static bool calculated_widths_;
  static int icon_only_width_;
  static int short_text_width_;
  static int full_text_width_;

  // Label strings for hint text and its short version (may be same).
  std::u16string hint_;
  std::u16string hint_short_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
