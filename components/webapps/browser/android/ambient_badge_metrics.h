// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_METRICS_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_METRICS_H_

#include "components/messages/android/message_enums.h"

namespace webapps {

// This enum backs a UMA histogram, so it should be treated as append-only.
enum class AppType {
  kNativeApp = 0,
  kWebApp = 1,
  kMaxValue = kWebApp,
};

void RecordAmbientBadgeDisplayEvent(bool native_app);
void RecordAmbientBadgeDismissEvent(bool native_app);
void RecordAmbientBadgeClickEvent(bool native_app);
void RecordAmbientBadgeMessageDismissReason(messages::DismissReason event);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_METRICS_H_