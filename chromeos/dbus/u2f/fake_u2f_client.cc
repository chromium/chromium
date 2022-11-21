// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/u2f/fake_u2f_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/cros_system_api/dbus/u2f/dbus-constants.h"

namespace chromeos {

// TODO(crbug/1150681): Make this fake more useful.

FakeU2FClient::FakeU2FClient() = default;
FakeU2FClient::~FakeU2FClient() = default;

void FakeU2FClient::IsUvpaa(const u2f::IsUvpaaRequest& request,
                            DBusMethodCallback<u2f::IsUvpaaResponse> callback) {
  u2f::IsUvpaaResponse response;
  response.set_available(false);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeU2FClient::IsU2FEnabled(
    const u2f::IsUvpaaRequest& request,
    DBusMethodCallback<u2f::IsUvpaaResponse> callback) {
  u2f::IsUvpaaResponse response;
  response.set_available(false);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeU2FClient::MakeCredential(
    const u2f::MakeCredentialRequest& request,
    DBusMethodCallback<u2f::MakeCredentialResponse> callback) {
  NOTREACHED();
}

void FakeU2FClient::GetAssertion(
    const u2f::GetAssertionRequest& request,
    DBusMethodCallback<u2f::GetAssertionResponse> callback) {
  NOTREACHED();
}

void FakeU2FClient::HasCredentials(
    const u2f::HasCredentialsRequest& request,
    DBusMethodCallback<u2f::HasCredentialsResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), u2f::HasCredentialsResponse()));
}

void FakeU2FClient::HasLegacyU2FCredentials(
    const u2f::HasCredentialsRequest& request,
    DBusMethodCallback<u2f::HasCredentialsResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), u2f::HasCredentialsResponse()));
}

void FakeU2FClient::CountCredentials(
    const u2f::CountCredentialsInTimeRangeRequest& request,
    DBusMethodCallback<u2f::CountCredentialsInTimeRangeResponse> callback) {
  NOTREACHED();
}

void FakeU2FClient::DeleteCredentials(
    const u2f::DeleteCredentialsInTimeRangeRequest& request,
    DBusMethodCallback<u2f::DeleteCredentialsInTimeRangeResponse> callback) {
  NOTREACHED();
}

void FakeU2FClient::CancelWebAuthnFlow(
    const u2f::CancelWebAuthnFlowRequest& request,
    DBusMethodCallback<u2f::CancelWebAuthnFlowResponse> callback) {
  NOTREACHED();
}

void FakeU2FClient::GetAlgorithms(
    const u2f::GetAlgorithmsRequest& request,
    DBusMethodCallback<u2f::GetAlgorithmsResponse> callback) {
  NOTREACHED();
}

void FakeU2FClient::GetSupportedFeatures(
    const u2f::GetSupportedFeaturesRequest& request,
    DBusMethodCallback<u2f::GetSupportedFeaturesResponse> callback) {
  NOTREACHED();
}

}  // namespace chromeos
