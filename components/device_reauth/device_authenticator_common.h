// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_COMMON_H_
#define COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_COMMON_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/keyed_service/core/keyed_service.h"

// Helper class which keeps the last good authentication timestamp such that it
// is common per profile.
class DeviceAuthenticatorProxy : public KeyedService {
 public:
  DeviceAuthenticatorProxy();
  ~DeviceAuthenticatorProxy() override;

  std::optional<base::TimeTicks> GetLastGoodAuthTimestamp() {
    return last_good_auth_timestamp_;
  }
  void UpdateLastGoodAuthTimestamp() {
    last_good_auth_timestamp_ = base::TimeTicks::Now();
  }
  base::WeakPtr<DeviceAuthenticatorProxy> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Time of last successful re-auth. nullopt if there hasn't been an auth yet.
  std::optional<base::TimeTicks> last_good_auth_timestamp_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<DeviceAuthenticatorProxy> weak_ptr_factory_{this};
};

// Used to care of the auth validity period for biometric authenticators.
class DeviceAuthenticatorCommon : public device_reauth::DeviceAuthenticator {
 public:
  DeviceAuthenticatorCommon(DeviceAuthenticatorProxy* proxy,
                            base::TimeDelta auth_validity_period,
                            const std::string& auth_result_histogram);

 protected:
  ~DeviceAuthenticatorCommon() override;

  // Checks whether user needs to reauthenticate.
  bool NeedsToAuthenticate() const;

  // Records the authentication time if the authentication was successful.
  void RecordAuthenticationTimeIfSuccessful(bool success);

  // Records ReauthResult::kSkipped for the `auth_result_histogram_` metric.
  void RecordAuthResultSkipped();

 private:
  // Used to obtain/update the last successful authentication timestamp.
  base::WeakPtr<DeviceAuthenticatorProxy> device_authenticator_proxy_;

  // Used to calculate how much time needs to pass before the user needs to
  // authenticate again.
  base::TimeDelta auth_validity_period_;

  // Used to record histograms compatible with the device_reauth::ReauthResult
  // enum.
  std::string auth_result_histogram_;
};

#endif  // COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_COMMON_H_
