// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_MODAL_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_MODAL_H_

#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

namespace safe_browsing {

// UMA histogram names for the modals.
const char kModalDisabledOutcome[] =
    "SafeBrowsing.TailoredSecurity.ConsentedDesktopModalDisabledOutcome";
const char kModalEnabledOutcome[] =
    "SafeBrowsing.TailoredSecurity.ConsentedDesktopModalEnabledOutcome";

static constexpr char kTailoredSecurityNoticeModal[] =
    "TailoredSecurityNoticeModal";

// Creates and shows a modal dialog for when Tailored Security is enabled.
void ShowEnabledModalForWebContents(content::WebContents* web_contents);

// Creates and shows a modal dialog for when Tailored Security is disabled.
void ShowDisabledModalForWebContents(content::WebContents* web_contents);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_MODAL_H_
