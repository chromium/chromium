// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/recent_tabs_dynamic_menu.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/actions.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"

RecentTabsDynamicMenu::RecentTabsDynamicMenu(BrowserWindowInterface* browser)
    : browser_window_interface_(browser) {}

RecentTabsDynamicMenu::~RecentTabsDynamicMenu() = default;

void RecentTabsDynamicMenu::BuildRecentTabsActions(
    actions::BaseAction* parent_item) {
  if (!parent_item || !browser_window_interface_) {
    return;
  }
  cancelable_task_tracker_.TryCancelAll();
  parent_item->ResetActionList();
  CreateRecentTabsAction(
      parent_item,
      RecentTabsBuilder::BuildRecentTabs(
          browser_window_interface_->GetProfile(), browser_window_interface_));
}

void RecentTabsDynamicMenu::ExecuteRecentTab(
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
            browser_window_interface_->GetActiveTabInterface()->GetContents(),
            *session_tab, disposition);
      }
    }
  }
}

void RecentTabsDynamicMenu::ExecuteRestoreEntry(
    SessionID id,
    WindowOpenDisposition disposition) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile());
  if (!service) {
    return;
  }
  sessions::LiveTabContext* live_context =
      BrowserLiveTabContext::From(browser_window_interface_);
  if (!live_context) {
    return;
  }
  service->RestoreEntryById(live_context, id, disposition);
}

void RecentTabsDynamicMenu::ExecuteRecentSplit(
    const RecentTabItem& recent_item,
    WindowOpenDisposition disposition,
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  ExecuteRestoreEntry(recent_item.session_id(), disposition);
}

actions::ActionItem::InvokeActionCallback
RecentTabsDynamicMenu::GetInvokeCallback(RecentTabItem recent_item,
                                         WindowOpenDisposition disposition) {
  switch (recent_item.type()) {
    case RecentTabItem::Type::kCommand: {
      if (recent_item.session_id().is_valid()) {
        return base::BindRepeating(&RecentTabsDynamicMenu::ExecuteRecentSplit,
                                   base::Unretained(this), recent_item,
                                   disposition);
      }
      break;
    }

    case RecentTabItem::Type::kTab: {
      return base::BindRepeating(&RecentTabsDynamicMenu::ExecuteRecentTab,
                                 base::Unretained(this), recent_item,
                                 disposition);
    }

    case RecentTabItem::Type::kWindow:
    case RecentTabItem::Type::kGroup:
    case RecentTabItem::Type::kSplit: {
      return base::BindRepeating(&RecentTabsDynamicMenu::ExecuteRecentSplit,
                                 base::Unretained(this), recent_item,
                                 disposition);
    }

    case RecentTabItem::Type::kHeader:
    case RecentTabItem::Type::kDevice:
      break;
  }
  return base::DoNothing();
}

void RecentTabsDynamicMenu::CreateRecentTabsAction(
    actions::BaseAction* parent_item,
    const std::vector<RecentTabItem>& recent_tabs) {
  if (!parent_item || recent_tabs.empty()) {
    return;
  }

  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  for (const auto& recent_tab : recent_tabs) {
    std::unique_ptr<actions::BaseAction> action_item;
    if (recent_tab.type() == RecentTabItem::Type::kHeader) {
      action_item = ActionAppMenuManager::CreateSectionHeaderActionItem(
          recent_tab.title());
    } else {
      if (recent_tab.action_id().has_value()) {
        action_item = ActionAppMenuManager::CreateIndirectActionItem(
            recent_tab.action_id().value(),
            ActionAppMenuManager::DisplayType::kRow);

        action_item.get()->GetActionItem()->SetText(recent_tab.title());
        action_item.get()->GetActionItem()->SetImage(recent_tab.icon());
        action_item.get()->GetActionItem()->SetProperty(
            ActionAppMenuManager::kContainerColorKey, ui::kColorMenuBackground);
        if (recent_tab.accelerator().has_value()) {
          action_item.get()->GetActionItem()->SetAccelerator(
              recent_tab.accelerator().value());
        }
      } else {
        auto builder = actions::ActionItem::Builder();
        builder.SetText(recent_tab.title())
            .SetImage(recent_tab.icon())
            .SetEnabled(recent_tab.enabled())
            .SetInvokeActionCallback(GetInvokeCallback(recent_tab, disposition))
            .SetProperty(ActionAppMenuManager::kContainerColorKey,
                         ui::kColorMenuBackground);
        if (recent_tab.accelerator().has_value()) {
          builder.SetAccelerator(recent_tab.accelerator().value());
        }

        action_item = std::move(builder).Build();
      }

      if (recent_tab.type() == RecentTabItem::Type::kTab &&
          !recent_tab.url().is_empty()) {
        FetchFavicon(action_item.get()->GetActionItem(), recent_tab);
      }
    }

    if (!recent_tab.children().empty()) {
      CreateRecentTabsAction(action_item.get(), recent_tab.children());
    }

    parent_item->AddChild(std::move(action_item));
  }
}

void RecentTabsDynamicMenu::FetchFavicon(actions::ActionItem* action_item,
                                         const RecentTabItem& item) {
  if (!browser_window_interface_ || !browser_window_interface_->GetProfile()) {
    return;
  }
  Profile* profile = browser_window_interface_->GetProfile();

  if (item.is_local()) {
    favicon::FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (favicon_service) {
      favicon_service->GetFaviconImageForPageURL(
          item.url(),
          base::BindOnce(&RecentTabsDynamicMenu::OnFaviconDataAvailable,
                         weak_ptr_factory_.GetWeakPtr(),
                         action_item->GetAsWeakPtr()),
          &cancelable_task_tracker_);
    }
  } else {
    favicon::HistoryUiFaviconRequestHandler*
        history_ui_favicon_request_handler =
            HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(
                profile);
    if (history_ui_favicon_request_handler) {
      history_ui_favicon_request_handler->GetFaviconImageForPageURL(
          item.url(),
          base::BindOnce(&RecentTabsDynamicMenu::OnFaviconDataAvailable,
                         weak_ptr_factory_.GetWeakPtr(),
                         action_item->GetAsWeakPtr()));
    }
  }
}

void RecentTabsDynamicMenu::OnFaviconDataAvailable(
    base::WeakPtr<actions::ActionItem> action_item,
    const favicon_base::FaviconImageResult& image_result) {
  if (action_item && !image_result.image.IsEmpty()) {
    action_item->SetImage(ui::ImageModel::FromImage(image_result.image));
  }
}
