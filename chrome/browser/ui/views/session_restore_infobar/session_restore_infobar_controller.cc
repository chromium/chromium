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
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace session_restore_infobar {

SessionRestoreInfobarController::SessionRestoreInfobarController(
    Profile& profile,
    bool was_restarted,
    bool is_post_crash_launch)
    : profile_(profile),
      model_(
          std::make_unique<SessionRestoreInfobarModel>(profile_.get(),
                                                       was_restarted,
                                                       is_post_crash_launch)) {}

SessionRestoreInfobarController::~SessionRestoreInfobarController() = default;

void SessionRestoreInfobarController::CreateOrDestroySessionRestoreInfobar(
    content::WebContents& web_contents) {
  if (!model_->ShouldShowOnStartup()) {
    return;
  }

  if (GetInfobarMessageType() ==
      SessionRestoreInfoBarDelegate::InfobarMessageType::kNone) {
    return;
  }

  session_restore_infobar::SessionRestoreInfoBarDelegate::Show(
      &web_contents, base::OnceCallback<void()>(), GetInfobarMessageType());
}

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
