// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/recent_tabs_dynamic_menu.h"

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
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "ui/actions/actions.h"

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
  CreateRecentTabsAction(parent_item,
                         RecentTabsBuilder::BuildRecentTabs(
                             browser_window_interface_->GetProfile(),
                             &browser_window_interface_->GetFeatures()));
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
            browser_window_interface_->tab_strip_model()
                ->GetActiveWebContents(),
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
  BrowserLiveTabContext* live_context =
      browser_window_interface_->GetFeatures().live_tab_context();
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
    const std::vector<RecentTabItem>& recent_items) {
  if (!parent_item || recent_items.empty()) {
    return;
  }

  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  for (const auto& current_item : recent_items) {
    std::unique_ptr<actions::BaseAction> action_item;
    if (current_item.type() == RecentTabItem::Type::kHeader) {
      action_item =
          std::make_unique<AppMenuSectionActionItem>(current_item.title());
    } else {
      if (current_item.action_id().has_value()) {
        action_item = ActionAppMenuManager::CreateIndirectActionItem(
            current_item.action_id().value(),
            ActionAppMenuManager::DisplayType::kRow,
            kColorAppMenuYourChromeBackground);

        action_item.get()->GetActionItem()->SetText(current_item.title());
        action_item.get()->GetActionItem()->SetImage(current_item.icon());
      } else {
        auto builder = actions::ActionItem::Builder();
        builder.SetText(current_item.title())
            .SetImage(current_item.icon())
            .SetEnabled(current_item.enabled())
            .SetInvokeActionCallback(
                GetInvokeCallback(current_item, disposition));

        action_item = std::move(builder).Build();
      }

      if (current_item.type() == RecentTabItem::Type::kTab &&
          !current_item.url().is_empty()) {
        FetchFavicon(action_item.get()->GetActionItem(), current_item);
      }
    }

    if (!current_item.children().empty()) {
      CreateRecentTabsAction(action_item.get(), current_item.children());
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
