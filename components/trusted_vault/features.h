// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_FEATURES_H_
#define COMPONENTS_TRUSTED_VAULT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace trusted_vault {

#if !BUILDFLAG(IS_ANDROID)
// Enables the chrome.setClientEncryptionKeys() JS API.
BASE_DECLARE_FEATURE(kSetClientEncryptionKeysJsApi);
#endif

#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kEnableICloudKeychainRecoveryFactor);
#endif

BASE_DECLARE_FEATURE(kEnableRegistrationStateSecurityDomainFiltering);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_FEATURES_H_
