// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_ORGANIZATION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_ORGANIZATION_BUTTON_H_

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class TabStripController;

class TabOrganizationButton : public TabStripControlButton {
 public:
  METADATA_HEADER(TabOrganizationButton);
  TabOrganizationButton(TabStripController* tab_strip_controller,
                        PressedCallback pressed_callback,
                        PressedCallback close_pressed_callback,
                        Edge flat_edge);
  TabOrganizationButton(const TabOrganizationButton&) = delete;
  TabOrganizationButton& operator=(const TabOrganizationButton&) = delete;
  ~TabOrganizationButton() override;

  void SetWidthFactor(float factor);
  float width_factor_for_testing() { return width_factor_; }

  // TabStripControlButton:
  gfx::Size CalculatePreferredSize() const override;

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

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_ORGANIZATION_BUTTON_H_
