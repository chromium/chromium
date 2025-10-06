// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_PREFS_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_PREFS_H_

class PrefService;

namespace session_restore_infobar {

// The maximum number of times the session restore infobar should be shown.
// Exposed for testing.
inline constexpr int kSessionRestoreInfoBarMaxTimesToShow = 3;

// Increments the number of times the infobar has been shown for `prefs`.
void IncrementInfoBarShownCount(PrefService* prefs);

// Returns true if the session restore infobar has been shown the maximum
// number of times allowed.
bool InfoBarShownMaxTimes(const PrefService* prefs);

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_PREFS_H_
