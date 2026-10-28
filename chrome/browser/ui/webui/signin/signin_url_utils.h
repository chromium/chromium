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
enum class ProfileCustomizationStyle {
  kDefault = 0,
  kLocalProfileCreation = 1
};

// Returns which style the profile customization page is using, as the default
// profile customization page or the local profile creation page.
ProfileCustomizationStyle GetProfileCustomizationStyle(const GURL& url);

// Adds the `style` URL query parameters to `url` for the profile customization.
GURL AppendProfileCustomizationQueryParams(const GURL& url,
                                           ProfileCustomizationStyle style);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
