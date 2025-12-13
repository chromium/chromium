// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace session_restore_infobar {

DEFINE_USER_DATA(SessionRestoreInfobarController);

SessionRestoreInfobarController::SessionRestoreInfobarController(
    BrowserWindowInterface* browser)
    : scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {}

SessionRestoreInfobarController::~SessionRestoreInfobarController() = default;

void SessionRestoreInfobarController::MaybeShowInfoBar(
    Profile& profile,
    bool is_post_crash_launch) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  if (host_content_settings_map->GetDefaultContentSetting(
          ContentSettingsType::COOKIES) != CONTENT_SETTING_ALLOW) {
    return;
  }

  model_ = std::make_unique<SessionRestoreInfobarModel>(profile,
                                                        is_post_crash_launch);

  if (InfoBarShownMaxTimes(profile.GetPrefs())) {
    return;
  }

  if (!profile.GetPrefs()
           ->FindPreference(prefs::kRestoreOnStartup)
           ->IsDefaultValue()) {
    return;
  }

  if (!model_->ShouldShowOnStartup()) {
    return;
  }
  if (GetInfobarMessageType() ==
      SessionRestoreInfoBarDelegate::InfobarMessageType::kNone) {
    return;
  }

  SessionRestoreInfoBarManager::GetInstance()->ShowInfoBar(
      profile, GetInfobarMessageType());
  IncrementInfoBarShownCount(profile.GetPrefs());
}

SessionRestoreInfobarController* SessionRestoreInfobarController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

SessionRestoreInfoBarDelegate::InfobarMessageType
SessionRestoreInfobarController::GetInfobarMessageType() {
  switch (model_->GetSessionRestoreMessageValue()) {
    case SessionRestoreInfobarModel::SessionRestoreMessageValue::
        kContinueWhereLeftOff:
        return SessionRestoreInfoBarDelegate::InfobarMessageType::
            kTurnOffFromRestart;
    case SessionRestoreInfobarModel::SessionRestoreMessageValue::kOpenNewTabPage:
      if (model_->IsDefaultSessionRestorePref()) {
        return SessionRestoreInfoBarDelegate::InfobarMessageType::
            kTurnOnSessionRestore;
      }
      return SessionRestoreInfoBarDelegate::InfobarMessageType::kNone;
    case SessionRestoreInfobarModel::SessionRestoreMessageValue::
        kOpenSpecificPages:
      return SessionRestoreInfoBarDelegate::InfobarMessageType::kNone;
  }
  NOTREACHED();
}

}  // namespace session_restore_infobar
