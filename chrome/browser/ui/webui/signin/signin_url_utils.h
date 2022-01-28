// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "components/signin/public/base/signin_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

// Holds parameters for the chrome://sync-confirmation URL.
// The default values correspond to the original usage of the sync confirmation
// page -- in the modal flow.
struct SyncConfirmationURLParams {
  bool is_modal = true;
  SyncConfirmationUI::DesignVersion design =
      SyncConfirmationUI::DesignVersion::kMonotone;
  absl::optional<SkColor> profile_color;
};

// Returns `SyncConfirmationURLParams` parsed from `url`.
SyncConfirmationURLParams GetParamsFromSyncConfirmationURL(const GURL& url);

// Adds URL query parameters specified by `params` to `url`.
GURL AppendSyncConfirmationQueryParams(const GURL& url,
                                       const SyncConfirmationURLParams& params);

// Returns `ReauthAccessPoint` encoded in the query of the reauth confirmation
// URL.
signin_metrics::ReauthAccessPoint GetReauthAccessPointForReauthConfirmationURL(
    const GURL& url);

// Returns a URL to display in the reauth confirmation dialog. The dialog was
// triggered by |access_point|.
GURL GetReauthConfirmationURL(signin_metrics::ReauthAccessPoint access_point);

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_URL_UTILS_H_
