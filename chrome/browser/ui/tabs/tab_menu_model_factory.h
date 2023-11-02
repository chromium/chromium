// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_FACTORY_H_

#include <memory>

#include "ui/base/models/simple_menu_model.h"

class TabStripModel;
class TabMenuModelDelegate;

// A factory to create menu models for tab menu.
class TabMenuModelFactory {
 public:
  TabMenuModelFactory() = default;
  TabMenuModelFactory(const TabMenuModelFactory&) = delete;
  virtual ~TabMenuModelFactory() = default;
  TabMenuModelFactory& operator=(const TabMenuModelFactory&) = delete;

  virtual std::unique_ptr<ui::SimpleMenuModel> Create(
      ui::SimpleMenuModel::Delegate* delegate,
      TabMenuModelDelegate* tab_menu_model_delegate,
      TabStripModel* tab_strip,
      int index);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_FACTORY_H_
