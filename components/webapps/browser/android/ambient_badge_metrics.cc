// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/ambient_badge_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace webapps {

namespace {

constexpr char kAmbientBadgeDisplayEventHistogram[] =
    "Webapp.AmbientBadge.Display";
constexpr char kAmbientBadgeDismissEventHistogram[] =
    "Webapp.AmbientBadge.Dismiss";
constexpr char kAmbientBadgeClickEventHistogram[] = "Webapp.AmbientBadge.Click";
constexpr char kAmbientBadgeMessageDismissReasonHistogram[] =
    "Webapp.AmbientBadge.Messages.DismissReason";

AppType getAppType(bool native_app) {
  return native_app ? AppType::kNativeApp : AppType::kWebApp;
}

}  // namespace

void RecordAmbientBadgeDisplayEvent(bool native_app) {
  base::UmaHistogramEnumeration(kAmbientBadgeDisplayEventHistogram,
                                getAppType(native_app));
}

void RecordAmbientBadgeDismissEvent(bool native_app) {
  base::UmaHistogramEnumeration(kAmbientBadgeDismissEventHistogram,
                                getAppType(native_app));
}

void RecordAmbientBadgeClickEvent(bool native_app) {
  base::UmaHistogramEnumeration(kAmbientBadgeClickEventHistogram,
                                getAppType(native_app));
}

void RecordAmbientBadgeMessageDismissReason(
    messages::DismissReason dismiss_reason) {
  base::UmaHistogramEnumeration(kAmbientBadgeMessageDismissReasonHistogram,
                                dismiss_reason, messages::DismissReason::COUNT);
}

}  // namespace webapps