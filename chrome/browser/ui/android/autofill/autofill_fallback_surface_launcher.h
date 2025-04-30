// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_FALLBACK_SURFACE_LAUNCHER_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_FALLBACK_SURFACE_LAUNCHER_H_

namespace content {
class WebContents;
}

namespace autofill {

// Opens the plus addresses management UI.
void ShowManagePlusAddressesPage(content::WebContents& web_contents);

// Opens the loyalty card management UI web page in a Chrome Custom Tab.
void ShowGoogleWalletLoyaltyCardsPage(content::WebContents& web_contents);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_FALLBACK_SURFACE_LAUNCHER_H_
