// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_prefs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace session_restore_infobar {

void IncrementInfoBarShownCount(PrefService* prefs) {
  prefs->SetInteger(
      prefs::kSessionRestoreInfoBarTimesShown,
      prefs->GetInteger(prefs::kSessionRestoreInfoBarTimesShown) + 1);
}

bool InfoBarShownMaxTimes(const PrefService* prefs) {
  return prefs->GetInteger(prefs::kSessionRestoreInfoBarTimesShown) >=
         kSessionRestoreInfoBarMaxTimesToShow;
}


}  // namespace session_restore_infobar
