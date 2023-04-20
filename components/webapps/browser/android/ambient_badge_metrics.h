// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_METRICS_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_METRICS_H_

#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/ambient_badge_manager.h"

namespace webapps {

void RecordAmbientBadgeDisplayEvent(AddToHomescreenParams::AppType type);
void RecordAmbientBadgeDismissEvent(AddToHomescreenParams::AppType type);
void RecordAmbientBadgeClickEvent(AddToHomescreenParams::AppType type);
void RecordAmbientBadgeTeminateState(AmbientBadgeManager::State state);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_METRICS_H_