// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_U2F_FAKE_U2F_CLIENT_H_
#define CHROMEOS_DBUS_U2F_FAKE_U2F_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/u2f/u2f_client.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS_U2F) FakeU2FClient : public U2FClient {
 public:
  FakeU2FClient();
  ~FakeU2FClient() override;

  FakeU2FClient(const FakeU2FClient&) = delete;
  FakeU2FClient& operator=(const FakeU2FClient&) = delete;

  // U2FClient:
  void IsUvpaa(const u2f::IsUvpaaRequest& request,
               DBusMethodCallback<u2f::IsUvpaaResponse> callback) override;
  void IsU2FEnabled(
      const u2f::IsU2fEnabledRequest& request,
      DBusMethodCallback<u2f::IsU2fEnabledResponse> callback) override;
  void MakeCredential(
      const u2f::MakeCredentialRequest& request,
      DBusMethodCallback<u2f::MakeCredentialResponse> callback) override;
  void GetAssertion(
      const u2f::GetAssertionRequest& request,
      DBusMethodCallback<u2f::GetAssertionResponse> callback) override;
  void HasCredentials(
      const u2f::HasCredentialsRequest& request,
      DBusMethodCallback<u2f::HasCredentialsResponse> callback) override;
  void HasLegacyU2FCredentials(
      const u2f::HasCredentialsRequest& request,
      DBusMethodCallback<u2f::HasCredentialsResponse> callback) override;
  void CountCredentials(
      const u2f::CountCredentialsInTimeRangeRequest& request,
      DBusMethodCallback<u2f::CountCredentialsInTimeRangeResponse> callback)
      override;
  void DeleteCredentials(
      const u2f::DeleteCredentialsInTimeRangeRequest& request,
      DBusMethodCallback<u2f::DeleteCredentialsInTimeRangeResponse> callback)
      override;
  void CancelWebAuthnFlow(
      const u2f::CancelWebAuthnFlowRequest& request,
      DBusMethodCallback<u2f::CancelWebAuthnFlowResponse> callback) override;
  void GetAlgorithms(
      const u2f::GetAlgorithmsRequest& request,
      DBusMethodCallback<u2f::GetAlgorithmsResponse> callback) override;
  void GetSupportedFeatures(
      const u2f::GetSupportedFeaturesRequest& request,
      DBusMethodCallback<u2f::GetSupportedFeaturesResponse> callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_U2F_FAKE_U2F_CLIENT_H_
