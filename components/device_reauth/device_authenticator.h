// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_
#define COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace device_reauth {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// The place where the device reauthentication flow is requested from.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class DeviceAuthSource {
  kPasswordManager = 0,
  kAutofill = 1,
  kIncognito = 2,
  kDeviceLockPage = 3,
  kMaxValue = kDeviceLockPage,
};

// When creating a device authenticator, one should create a |DeviceAuthParam|
// object, set its values and pass it as a parameter to
// ChromeDeviceAuthenticatorFactory::GetForProfile .
class DeviceAuthParams {
 public:
  DeviceAuthParams(base::TimeDelta auth_validity_period,
                   device_reauth::DeviceAuthSource source)
      : auth_validity_period_(auth_validity_period), source_(source) {}

  base::TimeDelta GetAuthenticationValidityPeriod() const {
    return auth_validity_period_;
  }
  device_reauth::DeviceAuthSource GetDeviceAuthSource() const {
    return source_;
  }

 private:
  base::TimeDelta auth_validity_period_;
  device_reauth::DeviceAuthSource source_;
};

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

  // Cancels an in-progress authentication if the filling surface requesting
  // the cancelation corresponds to the one for which the ongoing auth was
  // triggered.
  virtual void Cancel() = 0;
};

}  // namespace device_reauth

#endif  // COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_
