// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_CHROMEOS_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_CHROMEOS_H_

#include <stddef.h>

#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"

class Browser;
class TabStripModel;
class TabMenuModelDelegate;

namespace chromeos {

class DesksHelper;

// ExistingWindowSubMenuModel implementation for chromeos specific code.
class ExistingWindowSubMenuModelChromeOS : public ExistingWindowSubMenuModel {
 public:
  ExistingWindowSubMenuModelChromeOS(
      base::PassKey<ExistingWindowSubMenuModel> passkey,
      ui::SimpleMenuModel::Delegate* parent_delegate,
      TabMenuModelDelegate* tab_menu_model_delegate,
      TabStripModel* model,
      int context_index);
  ExistingWindowSubMenuModelChromeOS(
      const ExistingWindowSubMenuModelChromeOS&) = delete;
  ExistingWindowSubMenuModelChromeOS& operator=(
      const ExistingWindowSubMenuModelChromeOS&) = delete;
  ~ExistingWindowSubMenuModelChromeOS() override;

 private:
  // Builds the submenu, grouping browsers from |existing_browsers| by desks.
  // Browsers within desk groupings maintain the originally provided order. I.e.
  // if browsers were provided in MRU order, then within desk groupings browsers
  // will be in MRU order.
  void BuildMenuGroupedByDesk(const std::vector<Browser*>& existing_browsers);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_CHROMEOS_H_
