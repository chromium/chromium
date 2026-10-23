// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_FEATURES_H_
#define COMPONENTS_TTC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace ttc {

// Enables TTC feature.
BASE_DECLARE_FEATURE(kTtc);

// How long the backend has to respond to the session setup request before the
// session ends with an error.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kTtcSetupTimeout);

}  // namespace ttc

#endif  // COMPONENTS_TTC_FEATURES_H_
