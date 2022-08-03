// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
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

// Adds the following URL query parameters to `url`.
// `is_modal` specifies whether the style for modal dialog is used.
// `is_signin_intercept` specifies whether the style for the signin intercept is
// used.
GURL AppendSyncConfirmationQueryParams(const GURL& url,
                                       SyncConfirmationStyle style);

// Returns `ReauthAccessPoint` encoded in the query of the reauth confirmation
// URL.
signin_metrics::ReauthAccessPoint GetReauthAccessPointForReauthConfirmationURL(
    const GURL& url);

// Returns a URL to display in the reauth confirmation dialog. The dialog was
// triggered by |access_point|.
GURL GetReauthConfirmationURL(signin_metrics::ReauthAccessPoint access_point);

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
