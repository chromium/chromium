// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FEATURES_H_
#define COMPONENTS_GCM_DRIVER_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace gcm {

namespace features {

BASE_DECLARE_FEATURE(kInvalidateTokenFeature);
extern const char kParamNameTokenInvalidationPeriodDays[];

// Enables in-memory buffering of incoming push messages when no matching
// AppHandler is currently registered (e.g. during browser initialization).
BASE_DECLARE_FEATURE(kGCMMessageBuffering);

// The time-to-live for unhandled buffered push messages before they are pruned.
base::TimeDelta GetGCMMessageBufferingTTL();

// The period after which the GCM token becomes stale.
base::TimeDelta GetTokenInvalidationInterval();

}  // namespace features

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FEATURES_H_
