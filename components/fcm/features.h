// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_FEATURES_H_
#define COMPONENTS_FCM_FEATURES_H_

#include "base/feature_list.h"

namespace fcm {

// Controls whether FcmService is enabled instead of GCMDriver.
BASE_DECLARE_FEATURE(kUseFcmService);

}  // namespace fcm

#endif  // COMPONENTS_FCM_FEATURES_H_
