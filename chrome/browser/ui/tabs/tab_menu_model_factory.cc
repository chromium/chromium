// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"

#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"

std::unique_ptr<ui::SimpleMenuModel> TabMenuModelFactory::Create(
    ui::SimpleMenuModel::Delegate* delegate,
    TabMenuModelDelegate* tab_menu_model_delegate,
    TabStripModel* tab_strip,
    int index) {
  return std::make_unique<TabMenuModel>(delegate, tab_menu_model_delegate,
                                        tab_strip, index);
}

// Downcasts a `ui::SimpleMenuModel` to `TabMenuModel`. Since the default
// factory implementation always instantiates `TabMenuModel`, this downcast is
// safe. Subclasses that create other types of models (e.g., on ChromeOS) must
// override this to return nullptr.
TabMenuModel* TabMenuModelFactory::AsTabMenuModel(ui::SimpleMenuModel* model) {
  return static_cast<TabMenuModel*>(model);
}
