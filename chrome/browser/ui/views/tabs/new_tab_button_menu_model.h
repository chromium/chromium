// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_MENU_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"

class BrowserWindowInterface;

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

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_MENU_MODEL_H_
