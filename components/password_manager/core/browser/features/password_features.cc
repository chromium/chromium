// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_features.h"

namespace password_manager::features {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Enables biometric authentication before form filling.
BASE_FEATURE(kBiometricAuthenticationForFilling,
             "BiometricAuthenticationForFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
// Enables biometric authentication in settings.
BASE_FEATURE(kBiometricAuthenticationInSettings,
             "BiometricAuthenticationInSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
}  // namespace password_manager::features
