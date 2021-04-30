// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SWITCHES_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace switches {

extern const base::Feature kAllowSilentTrustedVaultDeviceRegistration;
extern const base::FeatureParam<base::TimeDelta>
    kTrustedVaultServiceThrottlingDuration;

}  // namespace switches

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_SWITCHES_H_
