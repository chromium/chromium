// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/device_reauth_util.h"

#include <memory>

#include "base/feature_list.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/device_reauth/device_auth_availability_checker.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"

namespace autofill {

bool SupportsDeviceReauth(const AutofillClient& client) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kDeviceReauthRefactor)) {
    std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker> checker =
        client.GetDeviceAuthAvailabilityChecker();
    return checker && checker->CanAuthenticateWithBiometricOrScreenLock();
  }
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
      client.GetDeviceAuthenticator();
  return authenticator &&
         authenticator->CanAuthenticateWithBiometricOrScreenLock();
}

}  // namespace autofill
