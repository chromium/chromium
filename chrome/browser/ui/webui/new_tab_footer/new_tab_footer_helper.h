// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_HELPER_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class Profile;

namespace ntp_footer {
// Returns whether `url` belongs to an extension NTP.
bool IsExtensionNtp(const GURL& url, Profile* profile);
// Returns whether the extension attribution can be shown.
bool CanShowExtensionFooter(const GURL& url, Profile* profile);
}  // namespace ntp_footer

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_HELPER_H_
