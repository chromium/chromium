// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/remote_tabs_menu_util.h"

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/vector_icon_types.h"

namespace remote_tabs_menu_util {

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

}  // namespace remote_tabs_menu_util
