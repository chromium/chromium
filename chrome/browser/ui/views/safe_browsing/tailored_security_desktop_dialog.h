// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_H_

class Browser;

namespace safe_browsing {

// UMA histogram names for the dialogs.
const char kDisabledDialogOutcome[] =
    "SafeBrowsing.TailoredSecurity.ConsentedDesktopDialogDisabledOutcome";
const char kEnabledDialogOutcome[] =
    "SafeBrowsing.TailoredSecurity.ConsentedDesktopDialogEnabledOutcome";

static constexpr char kTailoredSecurityNoticeDialog[] =
    "TailoredSecurityNoticeDialog";

// Creates and shows a dialog for when Tailored Security is enabled.
void ShowEnabledDialogForBrowser(Browser* browser);

// Creates and shows a dialog for when Tailored Security is disabled.
void ShowDisabledDialogForBrowser(Browser* browser);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_H_
