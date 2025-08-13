// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"
#include "components/prefs/pref_service.h"

namespace session_restore_infobar {

SessionRestoreInfobarController::SessionRestoreInfobarController()
    : model_(std::make_unique<SessionRestoreInfobarModel>(
          *GetProfile()->GetPrefs())) {}

SessionRestoreInfobarController::~SessionRestoreInfobarController() = default;

Profile* SessionRestoreInfobarController::GetProfile() {
  Profile* profile =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile()->GetProfile();
  CHECK(profile);
  return profile;
}

SessionRestoreInfobarModel::SessionRestoreMessageValue
SessionRestoreInfobarController::GetModelValue() {
  return model_->GetSessionRestoreMessageValue();
}

void SessionRestoreInfobarController::CreateOrDestroySessionRestoreInfobar() {
  // Get the profile and use it to get the message value.
  Profile* profile = GetProfile();

  SessionRestoreInfobarModel::SessionRestoreMessageValue message_value =
      GetModelValue();
  if (!CanShowInfobar(message_value, profile)) {
    return;
  }

  if (ShouldShowSessionRestoreInfobarOnStartup()) {
    model_->SetInfobarDelegate();
  }
}

bool SessionRestoreInfobarController::CanShowInfobar(
    SessionRestoreInfobarModel::SessionRestoreMessageValue message_value,
    Profile* profile) {
  return !(SessionRestore::IsRestoring(profile) ||
           message_value == SessionRestoreInfobarModel::
                                SessionRestoreMessageValue::OpenSpecificPages);
}

bool SessionRestoreInfobarController::
    ShouldShowSessionRestoreInfobarOnStartup() {
  // At startup, initiate the process to check session restore-related values.
  SessionRestoreInfobarModel::SessionRestoreMessageValue message_value =
      GetModelValue();
  bool continue_where_left_off =
      message_value == SessionRestoreInfobarModel::SessionRestoreMessageValue::
                           ContinueWhereLeftOff;

  bool open_new_tab_page =
      message_value ==
      SessionRestoreInfobarModel::SessionRestoreMessageValue::OpenNewTabPage;

  return continue_where_left_off || open_new_tab_page;
}

}  // namespace session_restore_infobar
