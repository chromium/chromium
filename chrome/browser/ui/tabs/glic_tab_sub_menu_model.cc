// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_tab_sub_menu_model.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

namespace {
constexpr int kMaxRecentConversations = 10;
}  // namespace

GlicTabSubMenuModel::GlicTabSubMenuModel(TabStripModel* tab_strip_model,
                                         int context_index)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      context_index_(context_index) {
  GlicKeyedService* glic_service =
      GlicKeyedService::Get(tab_strip_model_->profile());
  if (!glic_service) {
    return;
  }

  AddItem(TabStripModel::CommandGlicCreateNewChat,
          l10n_util::GetStringUTF16(IDS_TAB_CXMENU_GLIC_CREATE_NEW_CHAT));

  recent_conversations_ =
      glic_service->window_controller().GetRecentlyActiveInstances(
          kMaxRecentConversations);

  if (!recent_conversations_.empty()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    for (size_t i = 0; i < recent_conversations_.size(); ++i) {
      AddItem(kMinRecentConversationCommandId + i,
              base::UTF8ToUTF16(recent_conversations_[i].title));
    }
  }
}

GlicTabSubMenuModel::~GlicTabSubMenuModel() = default;

bool GlicTabSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool GlicTabSubMenuModel::IsCommandIdEnabled(int command_id) const {
  if (command_id >= kMinRecentConversationCommandId &&
      command_id <= kMaxRecentConversationCommandId) {
    return true;
  }
  return tab_strip_model_->IsContextMenuCommandEnabled(
      context_index_,
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

void GlicTabSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  std::vector<tabs::TabInterface*> tabs;
  if (tab_strip_model_->IsTabSelected(context_index_)) {
    const auto& selected_tabs =
        tab_strip_model_->selection_model().selected_tabs();
    tabs.assign(selected_tabs.begin(), selected_tabs.end());
  } else {
    tabs.push_back(tabs::TabInterface::GetFromContents(
        tab_strip_model_->GetWebContentsAt(context_index_)));
  }

  GlicKeyedService* service =
      GlicKeyedService::Get(tab_strip_model_->profile());
  if (!service) {
    return;
  }

  if (command_id == TabStripModel::CommandGlicCreateNewChat) {
    base::UmaHistogramCounts100(
        "Glic.TabContextMenu.PinnedTabsToNewConversation", tabs.size());
    service->window_controller().CreateNewConversationForTabs(tabs);
  } else if (command_id >= kMinRecentConversationCommandId &&
             command_id <= kMaxRecentConversationCommandId) {
    size_t conversation_index = command_id - kMinRecentConversationCommandId;
    CHECK_LT(conversation_index, recent_conversations_.size());
    base::UmaHistogramCounts100(
        "Glic.TabContextMenu.PinnedTabsToExistingConversation", tabs.size());
    service->window_controller().ShowInstanceForTabs(
        tabs, recent_conversations_[conversation_index].instance_id);
  }
}

}  // namespace glic
