// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_
#define COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace device_reauth {

// The UI surface requesting the device reauthentication flow.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class DeviceAuthRequester {
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

  // The prompt displayed when user is trying to copy/edit/view/export their
  // passwords from settings page on Windows and Mac.
  kPasswordsInSettings = 7,

  // The prompt displayed when the user is trying to autofill a local card, and
  // re-auth is triggered.
  kLocalCardAutofill = 8,

  // The page displayed to inform the user that a device lock is needed for
  // sign-in to protect their data privacy on this particular device. The
  // page will prompt the user to re-authenticate their existing device lock,
  // or create one if none exist.
  kDeviceLockPage = 9,

  // The prompt displayed when user is trying to enable/disable payment method
  // mandatory reauth toggle or edit local card when reauth is enabled in
  // Settings page on Android.
  kPaymentMethodsReauthInSettings = 10,

  // The prompt displayed when the user is trying to autofill a virtual card,
  // and re-auth is triggered.
  kVirtualCardAutofill = 11,

  // The prompt displayed while we were trying to opt the user into payments
  // autofill re-auth.
  kPaymentsAutofillOptIn = 12,

  kMaxValue = kPaymentsAutofillOptIn,
};

// This interface encapsulates operations related to biometric authentication.
// It's intended to be used prior to sharing the user's credentials with a
// website, either via form filling or the Credential Management API.
class DeviceAuthenticator : public base::RefCounted<DeviceAuthenticator> {
 public:
  using AuthenticateCallback = base::OnceCallback<void(bool)>;

  DeviceAuthenticator();
  DeviceAuthenticator(const DeviceAuthenticator&) = delete;
  DeviceAuthenticator& operator=(const DeviceAuthenticator&) = delete;

  // Returns whether biometrics are available for a given device.
  virtual bool CanAuthenticateWithBiometrics() = 0;

  // Returns whether biometrics or screenlock are available for a given device.
  virtual bool CanAuthenticateWithBiometricOrScreenLock() = 0;

  // Asks the user to authenticate. Invokes |callback| asynchronously when
  // the auth flow returns with the result.
  // |requester| is the filling surface that is asking for authentication.
  // |use_last_valid_auth| if set to false, ignores the grace 60 seconds
  // period between the last valid authentication and the current
  // authentication, and re-invokes system authentication.
  virtual void Authenticate(DeviceAuthRequester requester,
                            AuthenticateCallback callback,
                            bool use_last_valid_auth) = 0;

  // Asks the user to authenticate. Invokes |callback| asynchronously when
  // the auth flow returns with the result.
  // |message| contains text that will be displayed to the end user on
  // authentication request
  virtual void AuthenticateWithMessage(const std::u16string& message,
                                       AuthenticateCallback callback) = 0;

  // Cancels an in-progress authentication if the filling surface requesting
  // the cancelation corresponds to the one for which the ongoing auth was
  // triggered.
  virtual void Cancel(DeviceAuthRequester requester) = 0;

 protected:
  virtual ~DeviceAuthenticator() = default;

 private:
  friend class base::RefCounted<DeviceAuthenticator>;
};

}  // namespace device_reauth

#endif  // COMPONENTS_DEVICE_REAUTH_DEVICE_AUTHENTICATOR_H_
