// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_BOTTOM_CONTAINER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_BOTTOM_CONTAINER_BUTTON_H_

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/label_button.h"

// Button class that maintains the custom styling based on the vertical tab
// strip's collapsed state.
class BottomContainerButton : public views::LabelButton {
  METADATA_HEADER(BottomContainerButton, views::LabelButton)
 public:
  BottomContainerButton();
  ~BottomContainerButton() override = default;

  // views::LabelButton:
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_BOTTOM_CONTAINER_BUTTON_H_
