// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FEATURES_H_
#define COMPONENTS_GCM_DRIVER_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace gcm::features {

// Runs `GCMDriver` blocking storage tasks with `TaskPriority::USER_VISIBLE`
// instead of `TaskPriority::BEST_EFFORT` so key-store and database loads are
// not stalled behind `BEST_EFFORT` execution fences.
BASE_DECLARE_FEATURE(kGCMBlockingTaskRunnerUserVisible);

// Enables in-memory buffering of incoming push messages when no matching
// AppHandler is currently registered (e.g. during browser initialization).
BASE_DECLARE_FEATURE(kGCMMessageBuffering);

// The time-to-live for unhandled buffered push messages before they are pruned.
base::TimeDelta GetGCMMessageBufferingTTL();

}  // namespace gcm::features

#endif  // COMPONENTS_GCM_DRIVER_FEATURES_H_
