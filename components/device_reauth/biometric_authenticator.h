// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_BIOMETRIC_AUTHENTICATOR_H_
#define COMPONENTS_DEVICE_REAUTH_BIOMETRIC_AUTHENTICATOR_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace device_reauth {

// Different states for biometric availability for a given device. Either no
// biometric hardware is available, hardware is available but the user has no
// biometrics enrolled, or hardware is available and the user makes use of it.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class BiometricsAvailability {
  kOtherError = 0,
  kAvailable = 1,
  kAvailableNoFallback = 2,
  kNoHardware = 3,
  kHwUnavailable = 4,
  kNotEnrolled = 5,
  kSecurityUpdateRequired = 6,
  kAndroidVersionNotSupported = 7,

  kMaxValue = kAndroidVersionNotSupported,
};

// The filling surface asking for biometric authentication.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class BiometricAuthRequester {
  // The filling surface shown on the first tap on the field after page load.
  // This surface has replaced autofilling on Android.
  kTouchToFill = 0,

  // The suggestion presented in the keyboard accessory or autofill popup.
  kAutofillSuggestion = 1,

  // The keyboard accessory sheet displaying suggestions for manual filling.
  kFallbackSheet = 2,

  // The list displaying all saved passwords. Can be used for filling on
  // Android.
  kAllPasswordsList = 3,

  // The dialog displayed via the Credential Management API.
  kAccountChooserDialog = 4,

  // The list displaying all compromised passwords. Reauth is triggered before
  // starting automated password change.
  kPasswordCheckAutoPwdChange = 5,

  // The dialog displayed to access existing Incognito tabs if the Incognito
  // lock setting in on and Chrome came to foreground.
  kIncognitoReauthPage = 6,

  kMaxValue = kIncognitoReauthPage,
};

// The result of the biometric authentication.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BiometricAuthFinalResult {
  // This value is used for when we don't know the exact auth method used. This
  // can be the case on Android versions under 11.
  kSuccessWithUnknownMethod = 0,
  kSuccessWithBiometrics = 1,
  kSuccessWithDeviceLock = 2,
  kCanceledByUser = 3,
  kFailed = 4,

  // Deprecated in favour of kCanceledByChrome. Recorded when the auth succeeds
  // after Chrome cancelled it.
  // kSuccessButCanceled = 5,

  // Deprecated in favour of kCanceledByChrome. Recorded when the auth fails
  // after Chrome cancelled it.
  // kFailedAndCanceled = 6,

  // Recorded if an authentication was requested within 60s of the previous
  // successful authentication.
  kAuthStillValid = 7,

  // Recorded when the authentication flow is cancelled by Chrome.
  kCanceledByChrome = 8,

  kMaxValue = kCanceledByChrome,
};

// This interface encapsulates operations related to biometric authentication.
// It's intended to be used prior to sharing the user's credentials with a
// website, either via form filling or the Credential Management API.
class BiometricAuthenticator : public base::RefCounted<BiometricAuthenticator> {
 public:
  using AuthenticateCallback = base::OnceCallback<void(bool)>;

  BiometricAuthenticator();
  BiometricAuthenticator(const BiometricAuthenticator&) = delete;
  BiometricAuthenticator& operator=(const BiometricAuthenticator&) = delete;

  // Returns whether biometrics are available for a given device. Only if this
  // returns kAvailable, callers can expect Authenticate() to succeed.
  virtual BiometricsAvailability CanAuthenticate(
      BiometricAuthRequester requester) = 0;

  // Asks the user to authenticate. Invokes |callback| asynchronously when
  // the auth flow returns with the result.
  // |requester| is the filling surface that is asking for authentication.
  virtual void Authenticate(BiometricAuthRequester requester,
                            AuthenticateCallback callback) = 0;

  // Cancels an in-progress authentication if the filling surface requesting
  // the cancelation corresponds to the one for which the ongoing auth was
  // triggered.
  virtual void Cancel(BiometricAuthRequester requester) = 0;

 protected:
  virtual ~BiometricAuthenticator() = default;

 private:
  friend class base::RefCounted<BiometricAuthenticator>;
};

}  // namespace device_reauth

#endif  // COMPONENTS_DEVICE_REAUTH_BIOMETRIC_AUTHENTICATOR_H_
