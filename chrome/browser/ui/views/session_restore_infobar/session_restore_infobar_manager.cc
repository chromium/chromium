// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"

#include <memory>
#include <optional>
#include <ranges>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace session_restore_infobar {

void RecordInfoBarAction(InfobarMessageType message_type,
                         InfobarAction action) {
  switch (message_type) {
    case InfobarMessageType::kTurnOffFromRestart:
      base::UmaHistogramEnumeration("SessionRestore.InfoBar.TurnOffFromRestart",
                                    action);
      break;
    case InfobarMessageType::kTurnOnSessionRestore:
      base::UmaHistogramEnumeration(
          "SessionRestore.InfoBar.TurnOnSessionRestore", action);
      break;
    case InfobarMessageType::kNone:
      break;
  }
}

void RecordSettingChanged(bool setting_changed,
                          InfobarMessageType message_type) {
  switch (message_type) {
    case InfobarMessageType::kTurnOffFromRestart:
      base::UmaHistogramBoolean(
          "Session.Restore.SettingChanged.TurnOffFromRestart", setting_changed);
      break;
    case InfobarMessageType::kTurnOnSessionRestore:
      base::UmaHistogramBoolean(
          "Session.Restore.SettingChanged.TurnOnSessionRestore",
          setting_changed);
      break;
    case InfobarMessageType::kNone:
      break;
  }
}

// static
SessionRestoreInfoBarManager* SessionRestoreInfoBarManager::GetInstance() {
  return base::Singleton<SessionRestoreInfoBarManager>::get();
}

std::u16string SessionRestoreInfoBarManager::GetMessageText() const {
  switch (message_type_) {
    case InfobarMessageType::kTurnOffFromRestart:
      return l10n_util::GetStringUTF16(
          IDS_SESSION_RESTORE_TURN_OFF_RESTORE_FROM_RESTART);
    case InfobarMessageType::kTurnOnSessionRestore:
      return l10n_util::GetStringUTF16(IDS_SESSION_RESTORE_TURN_ON);
    case InfobarMessageType::kNone:
      return std::u16string();
  }
}

std::vector<MessageSubstitution>
SessionRestoreInfoBarManager::GetMessageSubstitutions() const {
  return {MessageSubstitution(
      GetMessageText(), /*is_link=*/false, /*accessible_name=*/std::nullopt)};
}

void SessionRestoreInfoBarManager::OnInfoBarResult(
    infobars::InfoBarResult result) {
  switch (result) {
    case infobars::InfoBarResult::kDismissed:
      action_taken_for_session_ = true;
      RecordInfoBarAction(message_type_, InfobarAction::kDismissed);
      if (profile_ && profile_->GetPrefs()
                          ->FindPreference(prefs::kRestoreOnStartup)
                          ->IsDefaultValue()) {
        RecordSettingChanged(false, message_type_);
      }
      CloseAllInfoBars();
      break;
    case infobars::InfoBarResult::kLinkClicked:
      action_taken_for_session_ = true;
      RecordInfoBarAction(message_type_, InfobarAction::kLinkClicked);
      break;
    case infobars::InfoBarResult::kIgnored:
      if (!action_taken_for_session_ && !ignored_metric_recorded_for_session_) {
        ignored_metric_recorded_for_session_ = true;
        RecordInfoBarAction(message_type_, InfobarAction::kIgnored);
        if (profile_ && profile_->GetPrefs()->GetInteger(
                            prefs::kSessionRestoreInfoBarTimesShown) ==
                            kSessionRestoreInfoBarMaxTimesToShow) {
          RecordSettingChanged(false, message_type_);
        }
      }
      break;
    case infobars::InfoBarResult::kAccepted:
    case infobars::InfoBarResult::kCancelled:
      break;
  }
}

void SessionRestoreInfoBarManager::ShowInfoBar(
    Profile& profile,
    InfobarMessageType message_type) {
  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE)) {
    if (profile_) {
      return;
    }

    profile_ = &profile;
    profile_->AddObserver(this);
    message_type_ = message_type;

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(profile_->GetPrefs());
    pref_change_registrar_->Add(
        prefs::kRestoreOnStartup,
        base::BindRepeating(
            &SessionRestoreInfoBarManager::OnSessionRestorePreferenceChanged,
            base::Unretained(this)));

    if (!shown_metric_recorded_for_session_) {
      shown_metric_recorded_for_session_ = true;
      ignored_metric_recorded_for_session_ = false;
      action_taken_for_session_ = false;
      RecordInfoBarAction(message_type_, InfobarAction::kShown);
    }

    auto* browser_infobar_manager =
        infobars::BrowserInfoBarManager::From(g_browser_process);
    if (browser_infobar_manager) {
      browser_infobar_manager->ShowGlobally(
          infobars::InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE);
    }
    return;
  }

  // Don't show a new infobar if one is already active.
  if (browser_tab_strip_tracker_) {
    return;
  }

  profile_ = &profile;
  profile_->AddObserver(this);
  message_type_ = message_type;

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kRestoreOnStartup,
      base::BindRepeating(
          &SessionRestoreInfoBarManager::OnSessionRestorePreferenceChanged,
          base::Unretained(this)));

  InitTabStripTracker();
}

void SessionRestoreInfoBarManager::CloseAllInfoBars() {
  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE)) {
    if (profile_) {
      profile_->RemoveObserver(this);
    }
    pref_change_registrar_.reset();
    profile_ = nullptr;
    user_initiated_info_bar_close_pending_ = false;
    message_type_ = InfobarMessageType::kNone;

    auto* browser_infobar_manager =
        infobars::BrowserInfoBarManager::From(g_browser_process);
    if (browser_infobar_manager) {
      browser_infobar_manager->Hide(
          infobars::InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE);
    }
    return;
  }

  if (profile_) {
    profile_->RemoveObserver(this);
  }

  browser_tab_strip_tracker_.reset();
  pref_change_registrar_.reset();

  // Repeatedly remove the first infobar. OnInfoBarRemoved will be called,
  // which erases the infobar from the map. This continues until the map is
  // empty.
  while (!infobars_.empty()) {
    infobars_.begin()->second->RemoveSelf();
  }

  CHECK(infobars_.empty());
  // Reset state to allow a new infobar to be shown in the future.
  profile_ = nullptr;
  user_initiated_info_bar_close_pending_ = false;
  message_type_ = InfobarMessageType::kNone;
}

SessionRestoreInfoBarManager::SessionRestoreInfoBarManager() = default;

SessionRestoreInfoBarManager::~SessionRestoreInfoBarManager() {
  // The browser process may be shutting down, so the profile may have already
  // been destroyed.
  if (profile_) {
    profile_->RemoveObserver(this);
  }
}

void SessionRestoreInfoBarManager::InitTabStripTracker() {
  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  // This will trigger a call to `OnTabStripModelChanged` for existing browsers,
  // which will create the info bar for their tabs.
  browser_tab_strip_tracker_->Init();
}

void SessionRestoreInfoBarManager::CreateInfoBarForWebContents(
    content::WebContents* web_contents) {
  // Ensure that an infobar hasn't already been created.
  CHECK(!infobars_.contains(web_contents));
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobars::InfoBar* infobar =
      session_restore_infobar::SessionRestoreInfoBarDelegate::Show(
          infobar_manager, *profile_,
          base::BindOnce(
              &SessionRestoreInfoBarManager::OnUserInitiatedInfoBarClose,
              base::Unretained(this)),
          message_type_);

  if (!infobar) {
    return;
  }

  infobars_[web_contents] = infobar;

  infobar_manager->AddObserver(this);
}

void SessionRestoreInfoBarManager::OnUserInitiatedInfoBarClose() {
  user_initiated_info_bar_close_pending_ = true;
}

bool SessionRestoreInfoBarManager::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  // Only show the infobar in normal, non-incognito, non-guest windows
  // that belong to the profile that triggered the session restore infobar.
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         profile_ && browser->GetProfile() == profile_ &&
         !browser->GetProfile()->IsOffTheRecord();
}

void SessionRestoreInfoBarManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (!infobars_.contains(contents.contents)) {
        CreateInfoBarForWebContents(contents.contents);
      }
    }
  }
}

void SessionRestoreInfoBarManager::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                    bool animate) {
  auto infobars_entry = std::ranges::find(
      infobars_, infobar, &decltype(infobars_)::value_type::second);
  if (infobars_entry == infobars_.end()) {
    return;
  }

  infobar->owner()->RemoveObserver(this);
  infobars_.erase(infobars_entry);

  // If this removal was triggered by user (Dismiss),
  // close all other infobars.
  if (user_initiated_info_bar_close_pending_) {
    CloseAllInfoBars();
  }
}

void SessionRestoreInfoBarManager::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK_EQ(profile, profile_);
  CloseAllInfoBars();
}

void SessionRestoreInfoBarManager::OnSessionRestorePreferenceChanged() {
  if (!profile_->GetPrefs()
           ->FindPreference(prefs::kRestoreOnStartup)
           ->IsDefaultValue()) {
    if (infobars::IsInfoBarMigrated(
            infobars::InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE)) {
      action_taken_for_session_ = true;
      RecordSettingChanged(true, message_type_);
    }
    CloseAllInfoBars();
  }
}

}  // namespace session_restore_infobar
