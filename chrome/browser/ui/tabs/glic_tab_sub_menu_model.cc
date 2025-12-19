// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_tab_sub_menu_model.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

GlicTabSubMenuModel::GlicTabSubMenuModel(TabStripModel* tab_strip_model,
                                         int context_index)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      context_index_(context_index) {
  AddItem(TabStripModel::CommandGlicCreateNewChat,
          l10n_util::GetStringUTF16(IDS_TAB_CXMENU_GLIC_CREATE_NEW_CHAT));
}

bool GlicTabSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool GlicTabSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return tab_strip_model_->IsContextMenuCommandEnabled(
      context_index_,
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

void GlicTabSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  tab_strip_model_->ExecuteContextMenuCommand(
      context_index_,
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

}  // namespace glic
