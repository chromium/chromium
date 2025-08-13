// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace session_restore_infobar {

SessionRestoreInfobarModel::~SessionRestoreInfobarModel() = default;

SessionRestoreInfobarModel::SessionRestoreInfobarModel(PrefService& prefs)
    : prefs_(prefs) {}

SessionRestoreInfobarModel::SessionRestoreMessageValue
SessionRestoreInfobarModel::GetSessionRestoreMessageValue() {
  // Get the integer value from the user's profile preferences.
  int restore_on_startup_value = prefs_->GetInteger(prefs::kRestoreOnStartup);
  // Get the value for chrome session restore.
  switch (restore_on_startup_value) {
    case 1:
      return ContinueWhereLeftOff;
    case 4:
      return OpenSpecificPages;
    case 5:
      return OpenNewTabPage;
    default:
      return OpenNewTabPage;
  }
}

void SessionRestoreInfobarModel::SetInfobarDelegate() {
  // TODO(crbug.com/431828875): Called to pass enum to the infobar delegate to
  // display the correct message in the infobar.
}

}  // namespace session_restore_infobar
