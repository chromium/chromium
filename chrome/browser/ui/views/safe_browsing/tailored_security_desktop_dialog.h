// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_H_

#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

namespace safe_browsing {

// UMA histogram names for the dialogs.
const char kDisabledDialogOutcome[] =
    "SafeBrowsing.TailoredSecurity.ConsentedDesktopDialogDisabledOutcome";
const char kEnabledDialogOutcome[] =
    "SafeBrowsing.TailoredSecurity.ConsentedDesktopDialogEnabledOutcome";

static constexpr char kTailoredSecurityNoticeDialog[] =
    "TailoredSecurityNoticeDialog";

// Creates and shows a dialog for when Tailored Security is enabled.
// TODO(crbug/1353914): remove unnecessary references to `web_contents`.
void ShowEnabledDialogForWebContents(Browser* browser,
                                     content::WebContents* web_contents);

// Creates and shows a dialog for when Tailored Security is disabled.
// TODO(crbug/1353914): remove unnecessary references to `web_contents`.
void ShowDisabledDialogForWebContents(Browser* browser,
                                      content::WebContents* web_contents);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_H_
