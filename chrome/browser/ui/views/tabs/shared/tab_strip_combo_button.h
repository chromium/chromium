// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace tab_groups {
class STGEverythingMenu;
}  // namespace tab_groups

namespace views {
class ActionViewController;
class MenuButtonController;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

class BrowserWindowInterface;
class TabStripFlatEdgeButton;

// A container for two TabStripFlatEdgeButtons that manages their flat edges
// based on visibility and the combo button's orientation.
class TabStripComboButton : public views::View,
                            public views::ContextMenuController,
                            public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(TabStripComboButton, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabSearchUnpinMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kProjectsPanelUnpinMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kEverythingMenuUnpinMenuItem);

  explicit TabStripComboButton(BrowserWindowInterface* browser);
  TabStripComboButton(const TabStripComboButton&) = delete;
  TabStripComboButton& operator=(const TabStripComboButton&) = delete;
  ~TabStripComboButton() override;

  void SetOrientation(views::LayoutOrientation orientation);

  TabStripFlatEdgeButton* start_button() { return start_button_; }
  TabStripFlatEdgeButton* end_button() { return end_button_; }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 protected:
  // views::View:
  void ChildVisibilityChanged(views::View* child) override;

 private:
  void ShowEverythingMenu();

  void UpdateButtonsVisibility();

  std::unique_ptr<TabStripFlatEdgeButton> CreateFlatEdgeButtonFor(
      actions::ActionId action_id,
      ui::ElementIdentifier element_id);

  void UpdateStyles();

  void OnMenuClosed();

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TabStripFlatEdgeButton> start_button_ = nullptr;
  raw_ptr<TabStripFlatEdgeButton> end_button_ = nullptr;
  views::LayoutOrientation orientation_ = views::LayoutOrientation::kHorizontal;

  PrefChangeRegistrar pref_registrar_;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<tab_groups::STGEverythingMenu> everything_menu_;
  raw_ptr<views::MenuButtonController> everything_menu_controller_ = nullptr;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_
