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
constexpr char kAmbientBadgeTerminateHistogram[] =
    "Webapp.AmbientBadge.Terminate";

}  // namespace

void RecordAmbientBadgeDisplayEvent(AddToHomescreenParams::AppType type) {
  base::UmaHistogramEnumeration(kAmbientBadgeDisplayEventHistogram, type,
                                AddToHomescreenParams::AppType::COUNT);
}

void RecordAmbientBadgeDismissEvent(AddToHomescreenParams::AppType type) {
  base::UmaHistogramEnumeration(kAmbientBadgeDismissEventHistogram, type,
                                AddToHomescreenParams::AppType::COUNT);
}

void RecordAmbientBadgeClickEvent(AddToHomescreenParams::AppType type) {
  base::UmaHistogramEnumeration(kAmbientBadgeClickEventHistogram, type,
                                AddToHomescreenParams::AppType::COUNT);
}

void RecordAmbientBadgeTeminateState(AmbientBadgeManager::State state) {
  base::UmaHistogramEnumeration(kAmbientBadgeTerminateHistogram, state);
}

}  // namespace webapps