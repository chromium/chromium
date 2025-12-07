// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

namespace views {
class MenuRunner;
}

class NewTabButtonMenuModel;

//  A subclass of TabStripControlButton that provides a specialized
// context menu to the new tab button for adding new tabs in
// groups or making new tab groups.
class NewTabButton : public TabStripControlButton,
                     public views::ContextMenuController {
 public:
  NewTabButton(TabStripController* tab_strip,
               PressedCallback callback,
               const gfx::VectorIcon& icon,
               Edge fixed_flat_edge = Edge::kNone,
               Edge animated_flat_edge = Edge::kNone,
               BrowserWindowInterface* browser = nullptr);

  NewTabButton(const NewTabButton&) = delete;
  NewTabButton& operator=(const NewTabButton&) = delete;
  ~NewTabButton() override;

  // views::ContextMenuController
  void ShowContextMenuForViewImpl(
      View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

 private:
  std::unique_ptr<NewTabButtonMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  raw_ptr<BrowserWindowInterface> browser_;
};

class NewTabButtonMenuModel : public ui::SimpleMenuModel,
                              public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kNewTab);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kNewTabInGroup);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kNewSplitView);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCreateNewTabGroup);
  explicit NewTabButtonMenuModel(BrowserWindowInterface* browser);
  NewTabButtonMenuModel(const NewTabButtonMenuModel&) = delete;
  NewTabButtonMenuModel& operator=(const NewTabButtonMenuModel&) = delete;
  ~NewTabButtonMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

 private:
  void AddNewTabInGroupItem();
  void AddNewSplitTabItem();

  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
