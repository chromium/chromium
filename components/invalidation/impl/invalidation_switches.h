// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace invalidation {
namespace switches {

BASE_DECLARE_FEATURE(kPolicyInstanceIDTokenTTL);
extern const base::FeatureParam<int> kPolicyInstanceIDTokenTTLSeconds;

}  // namespace switches
}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H_
