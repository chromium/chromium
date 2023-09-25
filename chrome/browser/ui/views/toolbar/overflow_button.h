// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_runner.h"

// A chevron button that indicates some toolbar elements have overflowed due to
// browser window being smaller than usual. Left press on it displays a drop
// down list of overflowed elements.
class OverflowButton : public ToolbarButton {
 public:
  METADATA_HEADER(OverflowButton);
  using CreateMenuModelCallback =
      base::RepeatingCallback<std::unique_ptr<ui::SimpleMenuModel>()>;

  OverflowButton();
  OverflowButton(const OverflowButton&) = delete;
  OverflowButton& operator=(const OverflowButton&) = delete;
  ~OverflowButton() override;

  // Triggered by left press.
  void RunMenu();

  void set_create_menu_model_callback(CreateMenuModelCallback callback) {
    create_menu_model_callback_ = std::move(callback);
  }

  const ui::SimpleMenuModel* menu_model_for_testing() const {
    return menu_model_.get();
  }

 private:
  base::RepeatingCallback<std::unique_ptr<ui::SimpleMenuModel>()>
      create_menu_model_callback_;

  // The menu of overflowed elements.
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  raw_ptr<views::MenuButtonController> menu_button_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_
