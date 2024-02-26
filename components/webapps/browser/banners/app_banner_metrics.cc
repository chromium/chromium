// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/app_banner_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace webapps {

const char kDismissEventHistogram[] = "AppBanners.DismissEvent";
const char kDisplayEventHistogram[] = "AppBanners.DisplayEvent";
const char kInstallEventHistogram[] = "AppBanners.InstallEvent";
const char kUserResponseHistogram[] = "AppBanners.UserResponse";
const char kBeforeInstallEventHistogram[] = "AppBanners.BeforeInstallEvent";
const char kInstallableStatusCodeHistogram[] =
    "AppBanners.InstallableStatusCode";
const char kInstallDisplayModeHistogram[] = "Webapp.Install.DisplayMode2";

void TrackDismissEvent(int event) {
  DCHECK_LT(DISMISS_EVENT_MIN, event);
  DCHECK_LT(event, DISMISS_EVENT_MAX);
  base::UmaHistogramSparse(kDismissEventHistogram, event);
}

void TrackDisplayEvent(int event) {
  DCHECK_LT(DISPLAY_EVENT_MIN, event);
  DCHECK_LT(event, DISPLAY_EVENT_MAX);
  base::UmaHistogramSparse(kDisplayEventHistogram, event);
}

void TrackInstallEvent(int event) {
  DCHECK_LT(INSTALL_EVENT_MIN, event);
  DCHECK_LT(event, INSTALL_EVENT_MAX);
  base::UmaHistogramSparse(kInstallEventHistogram, event);
}

void TrackUserResponse(int event) {
  DCHECK_LT(USER_RESPONSE_MIN, event);
  DCHECK_LT(event, USER_RESPONSE_MAX);
  base::UmaHistogramSparse(kUserResponseHistogram, event);
}

void TrackBeforeInstallEvent(int event) {
  DCHECK_LT(BEFORE_INSTALL_EVENT_MIN, event);
  DCHECK_LT(event, BEFORE_INSTALL_EVENT_MAX);
  base::UmaHistogramSparse(kBeforeInstallEventHistogram, event);
}

void TrackInstallableStatusCode(InstallableStatusCode code) {
  DCHECK_LE(InstallableStatusCode::NO_ERROR_DETECTED, code);
  DCHECK_LT(code, InstallableStatusCode::MAX_ERROR_CODE);
  if (code != InstallableStatusCode::IN_INCOGNITO) {
    // Do not log that we are in incognito to UMA.
    base::UmaHistogramSparse(kInstallableStatusCodeHistogram,
                             static_cast<int>(code));
  }
}

void TrackInstallDisplayMode(blink::mojom::DisplayMode display) {
  DCHECK_LE(blink::mojom::DisplayMode::kUndefined, display);
  DCHECK_LE(display, blink::mojom::DisplayMode::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION(kInstallDisplayModeHistogram, display);
}

}  // namespace webapps
