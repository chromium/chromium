// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_NUDGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_NUDGE_BUTTON_H_

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class TabStripController;

class TabStripNudgeButton : public TabStripControlButton {
  METADATA_HEADER(TabStripNudgeButton, TabStripControlButton)

 public:
  TabStripNudgeButton(TabStripController* tab_strip_controller,
                      PressedCallback pressed_callback,
                      PressedCallback close_pressed_callback,
                      const std::u16string& initial_label_text,
                      const ui::ElementIdentifier& element_identifier,
                      Edge flat_edge);

  TabStripNudgeButton(const TabStripNudgeButton&) = delete;
  TabStripNudgeButton& operator=(const TabStripNudgeButton&) = delete;
  ~TabStripNudgeButton() override;

  void SetOpacity(float opacity);
  void SetWidthFactor(float factor);
  float width_factor_for_testing() const { return width_factor_; }

  // TabStripControlButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::LabelButton* close_button_for_testing() { return close_button_; }

 protected:
  // TabStripControlButton:
  int GetCornerRadius() const override;
  int GetFlatCornerRadius() const override;

 private:
  void SetCloseButton(PressedCallback callback);

  // Preferred width multiplier, between 0-1. Used to animate button size.
  float width_factor_ = 0;
  raw_ptr<views::LabelButton> close_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_NUDGE_BUTTON_H_
