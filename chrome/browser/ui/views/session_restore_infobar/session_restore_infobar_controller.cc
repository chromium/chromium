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
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_prefs.h"
#include "content/public/browser/web_contents.h"

namespace session_restore_infobar {

void SessionRestoreInfobarController::MaybeShowInfoBar(
    Profile& profile,
    bool was_restarted,
    bool is_post_crash_launch) {
  model_ = std::make_unique<SessionRestoreInfobarModel>(profile, was_restarted,
                                                        is_post_crash_launch);
  if (InfoBarShownMaxTimes(profile.GetPrefs())) {
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

SessionRestoreInfobarController::SessionRestoreInfobarController() = default;

SessionRestoreInfobarController::~SessionRestoreInfobarController() = default;

SessionRestoreInfoBarDelegate::InfobarMessageType
SessionRestoreInfobarController::GetInfobarMessageType() {
  switch (model_->GetSessionRestoreMessageValue()) {
    case SessionRestoreInfobarModel::SessionRestoreMessageValue::
        ContinueWhereLeftOff:
      if (model_->IsBrowserRestarting()) {
        return SessionRestoreInfoBarDelegate::InfobarMessageType::
            kTurnOffFromRestart;
      } else {
        return SessionRestoreInfoBarDelegate::InfobarMessageType::
            kTurnOffFromSession;
      }
    case SessionRestoreInfobarModel::SessionRestoreMessageValue::OpenNewTabPage:
      if (model_->IsDefaultSessionRestorePref()) {
        return SessionRestoreInfoBarDelegate::InfobarMessageType::
            kTurnOnSessionRestore;
      }
      return SessionRestoreInfoBarDelegate::InfobarMessageType::kNone;
    case SessionRestoreInfobarModel::SessionRestoreMessageValue::
        OpenSpecificPages:
      return SessionRestoreInfoBarDelegate::InfobarMessageType::kNone;
  }
  NOTREACHED();
}

}  // namespace session_restore_infobar
