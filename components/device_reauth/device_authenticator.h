// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_
#define COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/device_reauth/device_reauth_metrics_util.h"

namespace device_reauth {

// When creating a device authenticator, one should create a |DeviceAuthParam|
// object, set its values and pass it as a parameter to
// ChromeDeviceAuthenticatorFactory::GetForProfile .
class DeviceAuthParams {
 public:
  DeviceAuthParams(base::TimeDelta auth_validity_period,
                   device_reauth::DeviceAuthSource source,
                   std::string auth_result_histogram = std::string())
      : auth_validity_period_(auth_validity_period),
        source_(source),
        auth_result_histogram_(auth_result_histogram) {}

  base::TimeDelta GetAuthenticationValidityPeriod() const {
    return auth_validity_period_;
  }
  device_reauth::DeviceAuthSource GetDeviceAuthSource() const {
    return source_;
  }
  const std::string& GetAuthResultHistogram() const {
    return auth_result_histogram_;
  }

 private:
  base::TimeDelta auth_validity_period_;
  device_reauth::DeviceAuthSource source_;
  // This histogram should be compatible with the device_reauth::ReauthResult
  // enum.
  std::string auth_result_histogram_;
};

#if BUILDFLAG(IS_ANDROID)
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class BiometricStatus {
  kRequired,
  kBiometricsAvailable,
  kOnlyLskfAvailable,
  kUnavailable,
};
#endif  // BUILDFLAG(IS_ANDROID)

// This interface encapsulates operations related to biometric authentication.
// It's intended to be used prior to sharing the user's credentials with a
// website, either via form filling or the Credential Management API.
class DeviceAuthenticator {
 public:
  using AuthenticateCallback = base::OnceCallback<void(bool)>;

  DeviceAuthenticator();
  DeviceAuthenticator(const DeviceAuthenticator&) = delete;
  virtual ~DeviceAuthenticator() = default;

  DeviceAuthenticator& operator=(const DeviceAuthenticator&) = delete;

  // Returns whether biometrics are available for a given device.
  virtual bool CanAuthenticateWithBiometrics() = 0;

  // Returns whether biometrics or screenlock are available for a given device.
  virtual bool CanAuthenticateWithBiometricOrScreenLock() = 0;

  // Asks the user to authenticate. Invokes |callback| asynchronously when
  // the auth flow returns with the result.
  // |message| contains text that will be displayed to the end user on
  // authentication request
  // On Android |message| is not relevant, can be empty.
  virtual void AuthenticateWithMessage(const std::u16string& message,
                                       AuthenticateCallback callback) = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual BiometricStatus GetBiometricAvailabilityStatus() = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  // Cancels an in-progress authentication if the filling surface requesting
  // the cancelation corresponds to the one for which the ongoing auth was
  // triggered.
  virtual void Cancel() = 0;
};

}  // namespace device_reauth

#endif  // COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_
