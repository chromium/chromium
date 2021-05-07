// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BIOMETRIC_AUTHENTICATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BIOMETRIC_AUTHENTICATOR_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace password_manager {

class UiCredential;

// Different states for biometric availability for a given device. Either no
// biometric hardware is available, hardware is available but the user has no
// biometrics enrolled, or hardware is available and the user makes use of it.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class BiometricsAvailability {
  kAvailable = 0,
  kNoHardware = 1,
  kNotEnrolled = 2,
  kAndroidVersionNotSupported = 3,
  kAvailableNoFallback = 4,
};

// This interface encapsulates operations related to biometric authentication.
// It's intended to be used prior to sharing the user's credentials with a
// website, either via form filling or the Credential Management API.
class BiometricAuthenticator : public base::RefCounted<BiometricAuthenticator> {
 public:
  using AuthenticateCallback = base::OnceCallback<void(bool)>;

  BiometricAuthenticator() = default;
  BiometricAuthenticator(const BiometricAuthenticator&) = delete;
  BiometricAuthenticator& operator=(const BiometricAuthenticator&) = delete;

  // Returns whether biometrics are available for a given device. Only if this
  // returns kAvailable, callers can expect Authenticate() to succeed.
  virtual BiometricsAvailability CanAuthenticate() = 0;

  // Asks the user to authenticate for the given |credential|. Invokes
  // |callback| asynchronously on the main thread with the result.
  virtual void Authenticate(const UiCredential& credential,
                            AuthenticateCallback callback) = 0;

  // Cancels an in-progress authentication.
  virtual void Cancel() = 0;

 protected:
  virtual ~BiometricAuthenticator() = default;

 private:
  friend class base::RefCounted<BiometricAuthenticator>;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BIOMETRIC_AUTHENTICATOR_H_
