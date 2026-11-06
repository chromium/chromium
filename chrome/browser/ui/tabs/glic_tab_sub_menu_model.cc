// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_tab_sub_menu_model.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

namespace {
constexpr int kMaxRecentConversations = 10;
}  // namespace

GlicTabSubMenuModel::GlicTabSubMenuModel(TabStripModel* tab_strip_model,
                                         tabs::TabHandle context_tab)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      context_tab_(context_tab) {
  GlicKeyedService* glic_service =
      GlicKeyedService::Get(tab_strip_model_->profile());
  if (!glic_service) {
    return;
  }

  AddItem(TabStripModel::CommandGlicCreateNewChat,
          l10n_util::GetStringUTF16(IDS_TAB_CXMENU_GLIC_CREATE_NEW_CHAT));

  recent_conversations_ =
      glic_service->instance_coordinator().GetRecentlyActiveInstances(
          kMaxRecentConversations, base::TimeDelta::Max());

  if (!recent_conversations_.empty()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    for (size_t i = 0; i < recent_conversations_.size(); ++i) {
      AddItem(kMinRecentConversationCommandId + i,
              base::UTF8ToUTF16(recent_conversations_[i].title));
    }
  }
}

GlicTabSubMenuModel::GlicTabSubMenuModel(TabStripModel* tab_strip_model,
                                         int context_index)
    : GlicTabSubMenuModel(
          tab_strip_model,
          tab_strip_model->ContainsIndex(context_index)
              ? tab_strip_model->GetTabAtIndex(context_index)->GetHandle()
              : tabs::TabHandle::Null()) {}

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
      context_tab_, static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

void GlicTabSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  std::vector<tabs::TabHandle> tabs =
      tab_strip_model_->GetTabsForCommand(context_tab_);

  GlicKeyedService* service =
      GlicKeyedService::Get(tab_strip_model_->profile());
  if (!service) {
    return;
  }

  if (command_id == TabStripModel::CommandGlicCreateNewChat ||
      (command_id >= kMinRecentConversationCommandId &&
       command_id <= kMaxRecentConversationCommandId)) {
    tabs::TabInterface* target_tab = context_tab_.Get();
    if (!target_tab) {
      return;
    }

    GlicInvokeOptions options(mojom::InvocationSource::kTabContextMenu);

    GlicMetricReporter metric_reporter(tab_strip_model_->profile());
    if (command_id == TabStripModel::CommandGlicCreateNewChat) {
      metric_reporter.RecordCounts100(
          "TabContextMenu.PinnedTabsToNewConversation", tabs.size());
      options.target = Target(*target_tab, NewConversation());
    } else {
      size_t conversation_index = command_id - kMinRecentConversationCommandId;
      CHECK_LT(conversation_index, recent_conversations_.size());
      metric_reporter.RecordCounts100(
          "TabContextMenu.PinnedTabsToExistingConversation", tabs.size());
      options.target = Target(
          *target_tab, recent_conversations_[conversation_index].instance_id);
    }

    options.tab_sharing = TabSharingOptions(tabs, GlicPinTrigger::kContextMenu);

    service->instance_coordinator().Invoke(std::move(options));
  }
}

}  // namespace glic
