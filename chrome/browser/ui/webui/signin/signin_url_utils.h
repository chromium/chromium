// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_

#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "url/gurl.h"

// Keep enum values in sync with the JS tests defined in signin_browsertest.js.
enum class SyncConfirmationStyle {
  kDefaultModal = 0,
  kSigninInterceptModal = 1,
  kWindow = 2
};

// Returns which style the sync confirmation page is using, as a default modal
// dialog, the signin intercept modal dialog version or as a window.
SyncConfirmationStyle GetSyncConfirmationStyle(const GURL& url);

// Returns true if the sync confirmation dialog is offered as an option, and
// false if the user explicitly initiated the flow.
bool IsSyncConfirmationPromo(const GURL& url);

// Adds the `style` URL query parameters to `url` for the sync confirmation.
// `is_sync_promo` is true if the sync confirmation dialog is offered as an
// option. It is false if the user explicitly initiated the flow.
GURL AppendSyncConfirmationQueryParams(const GURL& url,
                                       SyncConfirmationStyle style,
                                       bool is_sync_promo);

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

// TODO(crbug.com/381231566): `AddFromProfilePickerURLParameter()` is not called
// anymore and is now dead code, it should be removed in upcoming changes along
// with calls to `HasFromProfilePickerURLParameter()` and the dependant code.
//
// Checks if the |url| is coming from the ProfilePicker.
bool HasFromProfilePickerURLParameter(const GURL& url);
// Adds the ProfilePicker tag to the |url|. Returns the appended URL.
GURL AddFromProfilePickerURLParameter(const GURL& url);

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
