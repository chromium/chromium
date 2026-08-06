// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/app_menu_dynamic_helper.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"

AppMenuDynamicHelper::AppMenuDynamicHelper(BrowserWindowInterface* browser)
    : browser_window_interface_(browser) {}

AppMenuDynamicHelper::~AppMenuDynamicHelper() = default;

void AppMenuDynamicHelper::GetRecentTabsActions(
    actions::ActionItem* parent_item) {
  if (!parent_item) {
    return;
  }
  parent_item->ResetActionList();
  recent_tabs_list_ = RecentTabsBuilder::BuildRecentTabs(
      browser_window_interface_->GetProfile(),
      &browser_window_interface_->GetFeatures());
  CreateRecentTabsAction(parent_item, recent_tabs_list_);
}

void AppMenuDynamicHelper::ExecuteRecentTab(
    const RecentTabItem& recent_item,
    WindowOpenDisposition disposition,
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  if (recent_item.is_local()) {
    ExecuteRestoreEntry(recent_item.session_id(), disposition);
  } else {
    sync_sessions::SessionSyncService* session_sync_service =
        SessionSyncServiceFactory::GetInstance()->GetForProfile(
            browser_window_interface_->GetProfile());
    sync_sessions::OpenTabsUIDelegate* open_tabs =
        session_sync_service ? session_sync_service->GetOpenTabsUIDelegate()
                             : nullptr;
    if (open_tabs) {
      const sessions::SessionTab* session_tab = nullptr;
      if (open_tabs->GetForeignTab(recent_item.session_tag(),
                                   recent_item.session_id(), &session_tab) &&
          session_tab && !session_tab->navigations.empty()) {
        SessionRestore::RestoreForeignSessionTab(
            browser_window_interface_->tab_strip_model()
                ->GetActiveWebContents(),
            *session_tab, disposition);
      }
    }
  }
}

void AppMenuDynamicHelper::ExecuteRestoreEntry(
    SessionID id,
    WindowOpenDisposition disposition) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile());
  if (!service) {
    return;
  }
  BrowserLiveTabContext* live_context =
      browser_window_interface_->GetFeatures().live_tab_context();
  if (!live_context) {
    return;
  }
  service->RestoreEntryById(live_context, id, disposition);
}

void AppMenuDynamicHelper::ExecuteRecentSplit(
    const RecentTabItem& recent_item,
    WindowOpenDisposition disposition,
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  ExecuteRestoreEntry(recent_item.session_id(), disposition);
}

void AppMenuDynamicHelper::BindCallbackToBuilder(
    actions::ActionItem::ActionItemBuilder& builder,
    RecentTabItem recent_item,
    WindowOpenDisposition disposition) {
  switch (recent_item.type()) {
    case RecentTabItem::Type::kCommand: {
      if (recent_item.action_id().has_value()) {
        builder.SetActionId(recent_item.action_id().value());
      } else if (recent_item.session_id().is_valid()) {
        builder.SetInvokeActionCallback(base::BindRepeating(
            &AppMenuDynamicHelper::ExecuteRecentSplit, base::Unretained(this),
            recent_item, disposition));
      }
      break;
    }

    case RecentTabItem::Type::kTab: {
      builder.SetInvokeActionCallback(base::BindRepeating(
          &AppMenuDynamicHelper::ExecuteRecentTab, base::Unretained(this),
          recent_item, disposition));
      break;
    }

    case RecentTabItem::Type::kWindow:
    case RecentTabItem::Type::kGroup:
    case RecentTabItem::Type::kSplit: {
      builder.SetInvokeActionCallback(base::BindRepeating(
          &AppMenuDynamicHelper::ExecuteRecentSplit, base::Unretained(this),
          recent_item, disposition));
      break;
    }

    case RecentTabItem::Type::kHeader:
    case RecentTabItem::Type::kDevice:
      break;
  }
}

void AppMenuDynamicHelper::CreateRecentTabsAction(
    actions::BaseAction* parent_item,
    const std::vector<RecentTabItem>& recent_items) {
  if (!parent_item || recent_items.empty()) {
    return;
  }

  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  for (const auto& current_item : recent_items) {
    std::unique_ptr<actions::ActionItem> action_item;
    if (current_item.type() == RecentTabItem::Type::kHeader) {
      action_item =
          std::make_unique<AppMenuSectionActionItem>(current_item.title());
    } else {
      auto builder = actions::ActionItem::Builder();
      builder.SetText(current_item.title())
          .SetImage(current_item.icon())
          .SetEnabled(current_item.enabled());

      BindCallbackToBuilder(builder, current_item, disposition);

      action_item = std::move(builder).Build();
    }

    if (!current_item.children().empty()) {
      CreateRecentTabsAction(action_item.get(), current_item.children());
    }

    parent_item->AddChild(std::move(action_item));
  }
}
