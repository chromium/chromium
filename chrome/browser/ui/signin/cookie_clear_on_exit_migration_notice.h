// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_COOKIE_CLEAR_ON_EXIT_MIGRATION_NOTICE_H_
#define CHROME_BROWSER_UI_SIGNIN_COOKIE_CLEAR_ON_EXIT_MIGRATION_NOTICE_H_

#include "base/functional/callback_forward.h"

class Browser;

// Whether the cookie "clear on exit" migration notice should be shown: the
// migration is not completed, the notice is not currently shown, and the user
// is signed in.
bool CanShowCookieClearOnExitMigrationNotice(const Browser& browser);

// Factory function to create and show the cookie "clear on exit" migration
// notice. `callback` is called with true if the user choses to proceed with
// closing the windows, and false if closing the browser should be interrupted.
void ShowCookieClearOnExitMigrationNotice(
    Browser& browser,
    base::OnceCallback<void(bool)> callback);

#endif  // CHROME_BROWSER_UI_SIGNIN_COOKIE_CLEAR_ON_EXIT_MIGRATION_NOTICE_H_
