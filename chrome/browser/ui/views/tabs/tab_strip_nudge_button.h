// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_NUDGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_NUDGE_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class TabStripController;

namespace gfx {
class SlideAnimation;
}

class TabStripNudgeButton : public TabStripControlButton {
  METADATA_HEADER(TabStripNudgeButton, TabStripControlButton)

 public:
  TabStripNudgeButton(TabStripController* tab_strip_controller,
                      PressedCallback pressed_callback,
                      PressedCallback close_pressed_callback,
                      const std::u16string& initial_label_text,
                      const ui::ElementIdentifier& element_identifier,
                      Edge flat_edge,
                      const gfx::VectorIcon& icon,
                      const bool show_close_button);

  TabStripNudgeButton(const TabStripNudgeButton&) = delete;
  TabStripNudgeButton& operator=(const TabStripNudgeButton&) = delete;
  ~TabStripNudgeButton() override;

  void SetOpacity(float opacity);
  virtual void SetWidthFactor(float factor);
  float width_factor_for_testing() const { return width_factor_; }

  // TabStripControlButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::LabelButton* close_button_for_testing() { return close_button_; }

  float GetWidthFactor() const { return width_factor_; }

  virtual void SetIsShowingNudge(bool is_showing);

  bool GetIsShowingNudge() { return is_showing_nudge_; }

  virtual gfx::SlideAnimation* GetExpansionAnimationForTesting();

 protected:
  // TabStripControlButton:
  int GetCornerRadius() const override;
  int GetFlatCornerRadius() const override;
  void SetCloseButtonFocusBehavior(views::View::FocusBehavior focus_behavior);
  bool is_showing_nudge_ = false;

  views::View* close_button() { return close_button_; }

 private:
  void SetCloseButton(PressedCallback callback);
  float width_factor_ = 0;

  // Preferred width multiplier, between 0-1. Used to animate button size.
  raw_ptr<views::LabelButton> close_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_NUDGE_BUTTON_H_
