// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_U2F_U2F_CLIENT_H_
#define CHROMEOS_DBUS_U2F_U2F_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/u2f/u2f_interface.pb.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// U2FClient is used to communicate with the org.chromium.U2F service. The
// browser uses the U2F service to interface with the ChromeOS WebAuthn platform
// authenticator.
//
// The u2fd daemon implementing the U2F service is currently only available on
// devices with an H1 security chip. Callers should invoke
// IsU2FServiceAvailable() before calling any other methods.
class COMPONENT_EXPORT(CHROMEOS_DBUS_U2F) U2FClient {
 public:
  U2FClient(const U2FClient&) = delete;
  U2FClient& operator=(const U2FClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static U2FClient* Get();

  // IsU2FServiceAvailable checks with |TpmManagerClient| whether the device has
  // an H1 security chip (which is a proxy for "does it run u2fd?"). It runs
  // |callback| with the result.
  static void IsU2FServiceAvailable(
      base::OnceCallback<void(bool is_supported)> callback);

  // Returns whether the platform authenticator is configured (i.e. PIN is set
  // or fingerprint is enrolled). The name is short for
  // IsUserVerifyingPlatformAuthenticatorAvailable(), which is a method defined
  // in the WebAuthn spec.
  virtual void IsUvpaa(const u2f::IsUvpaaRequest& request,
                       DBusMethodCallback<u2f::IsUvpaaResponse> callback) = 0;

  // Returns whether the legacy enterprise policy to enable a U2F authenticator
  // that requires a power button press to register or sign with a credential is
  // enabled.
  virtual void IsU2FEnabled(
      const u2f::IsU2fEnabledRequest& request,
      DBusMethodCallback<u2f::IsU2fEnabledResponse> callback) = 0;

  // Registers a new credential.
  virtual void MakeCredential(
      const u2f::MakeCredentialRequest& request,
      DBusMethodCallback<u2f::MakeCredentialResponse> callback) = 0;

  // Challenges existing credentials.
  virtual void GetAssertion(
      const u2f::GetAssertionRequest& request,
      DBusMethodCallback<u2f::GetAssertionResponse> callback) = 0;

  // Returns whether a set of IDs belong to credentials registered by this
  // platform authenticator.
  virtual void HasCredentials(
      const u2f::HasCredentialsRequest& request,
      DBusMethodCallback<u2f::HasCredentialsResponse> callback) = 0;

  // Returns whether a set of IDs belong to credentials registered by the legacy
  // U2F authenticator.
  virtual void HasLegacyU2FCredentials(
      const u2f::HasCredentialsRequest& request,
      DBusMethodCallback<u2f::HasCredentialsResponse> callback) = 0;

  // Returns the number of credentials created within the specified time range.
  virtual void CountCredentials(
      const u2f::CountCredentialsInTimeRangeRequest& request,
      DBusMethodCallback<u2f::CountCredentialsInTimeRangeResponse>
          callback) = 0;

  // Deletes the credentials created within the specified time range and returns
  // the number of credentials deleted.
  virtual void DeleteCredentials(
      const u2f::DeleteCredentialsInTimeRangeRequest& request,
      DBusMethodCallback<u2f::DeleteCredentialsInTimeRangeResponse>
          callback) = 0;

  // Aborts a pending MakeCredential() or GetAssertion() request. Also
  // dismisses the OS UI dialog prompting the user to confirm the request
  // with a PIN or fingerprint.
  virtual void CancelWebAuthnFlow(
      const u2f::CancelWebAuthnFlowRequest& request,
      DBusMethodCallback<u2f::CancelWebAuthnFlowResponse> callback) = 0;

  virtual void GetAlgorithms(
      const u2f::GetAlgorithmsRequest& request,
      DBusMethodCallback<u2f::GetAlgorithmsResponse> callback) = 0;

  // Get supported features of u2fd. Currently only "whether lacros WebAuthn is
  // supported".
  virtual void GetSupportedFeatures(
      const u2f::GetSupportedFeaturesRequest& request,
      DBusMethodCallback<u2f::GetSupportedFeaturesResponse> callback) = 0;

 protected:
  U2FClient();
  virtual ~U2FClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_U2F_U2F_CLIENT_H_
