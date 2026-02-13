// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace tab_groups {
class STGEverythingMenu;
}  // namespace tab_groups

namespace views {
class ActionViewController;
class MenuButtonController;
}  // namespace views

class BrowserWindowInterface;
class TabStripFlatEdgeButton;

// A container for two TabStripFlatEdgeButtons that manages their flat edges
// based on visibility and the combo button's orientation.
class TabStripComboButton : public views::View {
  METADATA_HEADER(TabStripComboButton, views::View)
 public:
  explicit TabStripComboButton(BrowserWindowInterface* browser);
  TabStripComboButton(const TabStripComboButton&) = delete;
  TabStripComboButton& operator=(const TabStripComboButton&) = delete;
  ~TabStripComboButton() override;

  void SetOrientation(views::LayoutOrientation orientation);

  TabStripFlatEdgeButton* start_button() { return start_button_; }
  TabStripFlatEdgeButton* end_button() { return end_button_; }

 protected:
  // views::View:
  void ChildVisibilityChanged(views::View* child) override;

 private:
  void ShowEverythingMenu();

  std::unique_ptr<TabStripFlatEdgeButton> CreateFlatEdgeButtonFor(
      actions::ActionId action_id,
      ui::ElementIdentifier element_id);

  void UpdateStyles();

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TabStripFlatEdgeButton> start_button_ = nullptr;
  raw_ptr<TabStripFlatEdgeButton> end_button_ = nullptr;
  views::LayoutOrientation orientation_ = views::LayoutOrientation::kHorizontal;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<tab_groups::STGEverythingMenu> everything_menu_;
  raw_ptr<views::MenuButtonController> everything_menu_controller_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_
