// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_

#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "url/gurl.h"

// Launch modes supported by the history sync optin screen.
// Their choice also affects the style of the screen.
enum class HistorySyncOptinLaunchContext : int {
  kWindow = 0,
  kModal = 1,
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// LINT.IfChange(ProfileCustomizationStyle)
enum class ProfileCustomizationStyle {
  // Customization of a newly created signed-in profile.
  kSignedInProfileCreation = 0,
  // Customization of a newly created signed-out profile.
  kSignedOutProfileCreation = 1,
  // Editing of an existing profile.
  kEditProfile = 2,
};
// LINT.ThenChange(//chrome/browser/resources/signin/profile_customization/profile_customization_browser_proxy.ts:ProfileCustomizationStyle)

// Returns which style the profile customization page is using: the signed-in
// or signed-out profile creation, or the edit profile page.
ProfileCustomizationStyle GetProfileCustomizationStyle(const GURL& url);

// Returns whether the profile customization `style` is shown for a newly
// signed-in or created profile.
bool IsProfileCreationStyle(ProfileCustomizationStyle style);

// Adds the `style` URL query parameters to `url` for the profile customization.
GURL AppendProfileCustomizationQueryParams(const GURL& url,
                                           ProfileCustomizationStyle style);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
