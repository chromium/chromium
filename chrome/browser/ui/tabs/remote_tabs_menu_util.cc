// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/remote_tabs_menu_util.h"

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_types.h"
#include "components/sessions/core/session_types.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

namespace remote_tabs_menu_util {

namespace {

void OnRemoteTabFaviconAvailable(
    base::WeakPtr<ui::SimpleMenuModel> owner,
    base::WeakPtr<ui::MenuModel> menu_model,
    int command_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (!owner || !menu_model || image_result.image.IsEmpty()) {
    // Default icon has already been set.
    return;
  }
  static_cast<ui::SimpleMenuModel*>(menu_model.get())
      ->SetIconForCommandId(command_id,
                            ui::ImageModel::FromImage(image_result.image));
  if (ui::MenuModelDelegate* delegate = owner->menu_model_delegate()) {
    delegate->OnIconChanged(command_id);
  }
}

}  // namespace

bool ShouldShowTabsFromOtherDevices(Profile* profile) {
#if !BUILDFLAG(IS_CHROMEOS)
  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile);
    if (!sync_service) {
      return false;
    }

    if (!signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
            sync_service, {syncer::UserSelectableType::kTabs})) {
      // Tabs from other devices should not be offered if syncing tabs is
      // disabled by policy.
      return false;
    }

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    switch (signin_util::GetSignedInState(identity_manager)) {
      case signin_util::SignedInState::kSignedIn:
      case signin_util::SignedInState::kSignInPending:
        if (signin_util::HasExplicitlyDisabledHistorySync(sync_service,
                                                          identity_manager)) {
          // Tabs from other devices should not be offered if syncing tabs is
          // explicitly disabled by the user.
          return false;
        }
        break;
      case signin_util::SignedInState::kSignedOut:
      case signin_util::SignedInState::kWebOnlySignedIn:
      case signin_util::SignedInState::kSyncing:
      case signin_util::SignedInState::kSyncPaused:
        break;
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  return true;
}

ui::ImageModel GetDeviceFormFactorIcon(
    syncer::DeviceInfo::FormFactor form_factor) {
  const gfx::VectorIcon* icon = nullptr;
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kPhone:
      icon = &(features::IsRoundedIconsEnabled() ? kMobileIcon
                                                 : kSmartphoneOldIcon);
      break;
    case syncer::DeviceInfo::FormFactor::kTablet:
      icon = &(features::IsRoundedIconsEnabled() ? kTabletFilledIcon
                                                 : kTabletOldIcon);
      break;
    // Use the laptop icon as default.
    case syncer::DeviceInfo::FormFactor::kUnknown:
    case syncer::DeviceInfo::FormFactor::kAutomotive:
    case syncer::DeviceInfo::FormFactor::kWearable:
    case syncer::DeviceInfo::FormFactor::kTv:
    case syncer::DeviceInfo::FormFactor::kDesktop:
      icon = &(features::IsRoundedIconsEnabled() ? kLaptopWindowsIcon
                                                 : kLaptopOldIcon);
      break;
  }
  return ui::ImageModel::FromVectorIcon(*icon, ui::kColorMenuIcon,
                                        gfx::kFaviconSize);
}

void AddRemoteTabFavicon(Profile* profile,
                         base::WeakPtr<ui::SimpleMenuModel> owner,
                         ui::SimpleMenuModel* menu_model,
                         int command_id,
                         const GURL& url) {
  menu_model->SetIconForCommandId(command_id,
                                  favicon::GetDefaultFaviconModel());

  favicon::HistoryUiFaviconRequestHandler* history_ui_favicon_request_handler =
      HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(profile);
  // Can be null for tests.
  if (!history_ui_favicon_request_handler) {
    return;
  }
  history_ui_favicon_request_handler->GetFaviconImageForPageURL(
      url, base::BindOnce(&OnRemoteTabFaviconAvailable, std::move(owner),
                          menu_model->AsWeakPtr(), command_id));
}

bool OpenForeignSessionTab(BrowserWindowInterface* browser,
                           sync_sessions::OpenTabsUIDelegate* open_tabs,
                           const std::string& session_tag,
                           SessionID tab_id,
                           WindowOpenDisposition disposition,
                           base::UserMetricsAction user_action) {
  if (!open_tabs) {
    return false;
  }
  const sessions::SessionTab* session_tab = nullptr;
  if (!open_tabs->GetForeignTab(session_tag, tab_id, &session_tab)) {
    return false;
  }
  if (session_tab->navigations.empty()) {
    return false;
  }
  base::RecordAction(user_action);
  SessionRestore::RestoreForeignSessionTab(
      browser->GetTabStripModel()->GetActiveWebContents(), *session_tab,
      disposition);
  return true;
}

}  // namespace remote_tabs_menu_util
