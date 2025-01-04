// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/compare_sub_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "components/strings/grit/components_strings.h"

namespace commerce {

CompareSubMenuModel::CompareSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  AddItemWithStringId(IDC_SHOW_ALL_COMPARISON_TABLES,
                      IDS_COMPARE_SHOW_ALL_COMPARISON_TABLES);
}

}  // namespace commerce
