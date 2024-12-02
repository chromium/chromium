// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/local_credential_management_mac.h"
#include "chrome/browser/webauthn/local_credential_management.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "device/fido/mac/credential_store.h"

LocalCredentialManagementMac::LocalCredentialManagementMac(
    device::fido::mac::AuthenticatorConfig config)
    : config_(config) {}

std::unique_ptr<LocalCredentialManagement> LocalCredentialManagement::Create(
    Profile* profile) {
  auto config =
      ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfigForProfile(
          profile);
  return std::make_unique<LocalCredentialManagementMac>(config);
}

void LocalCredentialManagementMac::HasCredentials(
    base::OnceCallback<void(bool)> callback) {
  Enumerate(
      base::BindOnce(
          [](std::optional<std::vector<device::DiscoverableCredentialMetadata>>
                 metadata) { return metadata ? !metadata->empty() : false; })
          .Then(std::move(callback)));
}

void LocalCredentialManagementMac::Enumerate(
    base::OnceCallback<void(
        std::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
        callback) {
  device::fido::mac::TouchIdCredentialStore credential_store(config_);
  std::optional<std::list<device::fido::mac::Credential>> credentials =
      credential_store.FindResidentCredentials(/*rp_id=*/std::nullopt);

  if (!credentials) {
    // FindResidentCredentials() encountered an error.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  std::vector<device::DiscoverableCredentialMetadata> credential_metadata;
  credential_metadata.reserve(credentials->size());
  for (auto& credential : *credentials) {
    credential_metadata.emplace_back(
        device::AuthenticatorType::kTouchID, credential.rp_id,
        credential.credential_id,
        credential.metadata.ToPublicKeyCredentialUserEntity());
  }
  std::sort(credential_metadata.begin(), credential_metadata.end(),
            CredentialComparator());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(credential_metadata)));
}

void LocalCredentialManagementMac::Delete(
    base::span<const uint8_t> credential_id,
    base::OnceCallback<void(bool)> callback) {
  device::fido::mac::TouchIdCredentialStore credential_store(config_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     credential_store.DeleteCredentialById(credential_id)));
}

void LocalCredentialManagementMac::Edit(
    base::span<uint8_t> credential_id,
    std::string new_username,
    base::OnceCallback<void(bool)> callback) {
  device::fido::mac::TouchIdCredentialStore credential_store(config_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), credential_store.UpdateCredential(
                                              credential_id, new_username)));
}
