// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace session_restore_infobar {

namespace {

void RecordInfoBarAction(
    SessionRestoreInfoBarDelegate::InfobarMessageType message_type,
    SessionRestoreInfoBarDelegate::InfobarAction action) {
  switch (message_type) {
    case SessionRestoreInfoBarDelegate::InfobarMessageType::kTurnOffFromRestart:
      base::UmaHistogramEnumeration("SessionRestore.InfoBar.TurnOffFromRestart",
                                    action);
      break;
    case SessionRestoreInfoBarDelegate::InfobarMessageType::
        kTurnOnSessionRestore:
      base::UmaHistogramEnumeration(
          "SessionRestore.InfoBar.TurnOnSessionRestore", action);
      break;
    case SessionRestoreInfoBarDelegate::InfobarMessageType::kNone:
      break;
  }
}

}  // namespace

// static
infobars::InfoBar* SessionRestoreInfoBarDelegate::Show(
    infobars::ContentInfoBarManager* infobar_manager,
    Profile& profile,
    base::OnceCallback<void()> close_cb,
    SessionRestoreInfoBarDelegate::InfobarMessageType message_type) {
  auto* manager = SessionRestoreInfoBarManager::GetInstance();
  if (!manager->shown_metric_recorded_for_session()) {
    manager->set_shown_metric_recorded_for_session(true);
    manager->set_ignored_metric_recorded_for_session(false);
    manager->set_action_taken_for_session(false);
    RecordInfoBarAction(message_type,
                        SessionRestoreInfoBarDelegate::InfobarAction::kShown);
  }
  std::unique_ptr<SessionRestoreInfoBarDelegate> delegate =
      std::make_unique<SessionRestoreInfoBarDelegate>(
          profile, std::move(close_cb), message_type);
  return infobar_manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));
}

SessionRestoreInfoBarDelegate::SessionRestoreInfoBarDelegate(
    Profile& profile,
    base::OnceCallback<void()> close_cb,
    SessionRestoreInfoBarDelegate::InfobarMessageType message_type)
    : profile_(profile),
      close_cb_(std::move(close_cb)),
      message_type_(message_type) {
  pref_change_registrar_.Init(profile_.get().GetPrefs());
  pref_change_registrar_.Add(
      prefs::kRestoreOnStartup,
      base::BindRepeating(
          &SessionRestoreInfoBarDelegate::OnSessionRestorePrefChanged,
          base::Unretained(this)));
}

SessionRestoreInfoBarDelegate::~SessionRestoreInfoBarDelegate() {
  auto* manager = SessionRestoreInfoBarManager::GetInstance();
  if (!action_taken_) {
    if (manager->action_taken_for_session()) {
      return;
    }
    if (!manager->ignored_metric_recorded_for_session()) {
      manager->set_ignored_metric_recorded_for_session(true);
      RecordInfoBarAction(
          message_type_,
          SessionRestoreInfoBarDelegate::InfobarAction::kIgnored);
      if (profile_->GetPrefs()->GetInteger(
              prefs::kSessionRestoreInfoBarTimesShown) ==
          kSessionRestoreInfoBarMaxTimesToShow) {
        // The session restore infobar will be shown
        // on 3 different browser sessions. And false will be recorded once if
        // no action was taken to change the setting.
        RecordSettingChanged(false, message_type_);
      }
    }
  }
}

void SessionRestoreInfoBarDelegate::OnSessionRestorePrefChanged() {
  SessionRestoreInfoBarManager::GetInstance()->set_action_taken_for_session(
      true);
  RecordSettingChanged(true, message_type_);
  infobar()->RemoveSelf();
}

void SessionRestoreInfoBarDelegate::RecordSettingChanged(
    bool setting_changed,
    SessionRestoreInfoBarDelegate::InfobarMessageType message_type) {
  switch (message_type) {
    case SessionRestoreInfoBarDelegate::InfobarMessageType::kTurnOffFromRestart:
      base::UmaHistogramBoolean(
          "Session.Restore.SettingChanged.TurnOffFromRestart", setting_changed);
      break;
    case SessionRestoreInfoBarDelegate::InfobarMessageType::
        kTurnOnSessionRestore:
      base::UmaHistogramBoolean(
          "Session.Restore.SettingChanged.TurnOnSessionRestore",
          setting_changed);
      break;
    case SessionRestoreInfoBarDelegate::InfobarMessageType::kNone:
      break;
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
SessionRestoreInfoBarDelegate::GetIdentifier() const {
  return infobars::InfoBarDelegate::InfoBarIdentifier::
      SESSION_RESTORE_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& SessionRestoreInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
                     : vector_icons::kProductRefreshIcon;
}

bool SessionRestoreInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Returns false if the infobar should not be dismissed on navigation.
  return false;
}

std::u16string SessionRestoreInfoBarDelegate::GetMessageText() const {
  switch (message_type_) {
    case InfobarMessageType::kTurnOffFromRestart:
      return l10n_util::GetStringUTF16(
          IDS_SESSION_RESTORE_TURN_OFF_RESTORE_FROM_RESTART);
    case InfobarMessageType::kTurnOnSessionRestore:
      return l10n_util::GetStringUTF16(IDS_SESSION_RESTORE_TURN_ON);
    case InfobarMessageType::kNone:
      return std::u16string();
  }
  return std::u16string();
}

std::u16string SessionRestoreInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_SESSION_RESTORE_LINK);
}

int SessionRestoreInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

bool SessionRestoreInfoBarDelegate::ShouldShowLinkBeforeButton() const {
  return true;
}

int SessionRestoreInfoBarDelegate::GetLinkSpacingWhenPositionedBeforeButton()
    const {
  return 4;
}

void SessionRestoreInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  SessionRestoreInfoBarManager::GetInstance()->set_action_taken_for_session(
      true);
  RecordInfoBarAction(message_type_,
                      SessionRestoreInfoBarDelegate::InfobarAction::kDismissed);
  if (close_cb_) {
    std::move(close_cb_).Run();
    if (profile_->GetPrefs()
            ->FindPreference(prefs::kRestoreOnStartup)
            ->IsDefaultValue()) {
      RecordSettingChanged(false, message_type_);
    }
  }
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

GURL SessionRestoreInfoBarDelegate::GetLinkURL() const {
  const std::string learn_more_url_str = "chrome://settings/onStartup";
  GURL learn_more_url(learn_more_url_str);
  CHECK(learn_more_url.is_valid());
  return learn_more_url;
}

bool SessionRestoreInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  action_taken_ = true;
  SessionRestoreInfoBarManager::GetInstance()->set_action_taken_for_session(
      true);
  RecordInfoBarAction(
      message_type_,
      SessionRestoreInfoBarDelegate::InfobarAction::kLinkClicked);
  return ConfirmInfoBarDelegate::LinkClicked(disposition);
}

std::optional<std::u16string>
SessionRestoreInfoBarDelegate::GetLinkAccessibleText() const {
  return l10n_util::GetStringUTF16(IDS_SESSION_RESTORE_LINK_ARIA_LABEL);
}

}  // namespace session_restore_infobar
