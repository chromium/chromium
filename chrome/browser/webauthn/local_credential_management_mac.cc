// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/webauthn/local_credential_management_mac.h"
#include "chrome/browser/webauthn/local_credential_management.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"

LocalCredentialManagementMac::LocalCredentialManagementMac() = default;

std::unique_ptr<LocalCredentialManagement> LocalCredentialManagement::Create() {
  return std::make_unique<LocalCredentialManagementMac>();
}

void LocalCredentialManagementMac::HasCredentials(
    Profile* profile,
    base::OnceCallback<void(bool)> callback) {
  if (!base::FeatureList::IsEnabled(features::kWebAuthConditionalUI)) {
    std::move(callback).Run(false);
    return;
  }
  Enumerate(
      profile,
      base::BindOnce(
          [](absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
                 metadata) { return !metadata->empty(); })
          .Then(std::move(callback)));
}

void LocalCredentialManagementMac::Enumerate(
    Profile* profile,
    base::OnceCallback<void(
        absl::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
        callback) {
  // TODO(crbug.com/1349854): wire up the real keychain implementation.
  device::DiscoverableCredentialMetadata cred_1(
      "a.com", {0}, device::PublicKeyCredentialUserEntity({1, 2, 3, 4}));
  device::DiscoverableCredentialMetadata cred_2(
      "b.com", {1}, device::PublicKeyCredentialUserEntity({5, 6, 7, 8}));
  std::vector<device::DiscoverableCredentialMetadata> credential_metadata;

  credential_metadata.push_back(cred_1);
  credential_metadata.push_back(cred_2);

  std::move(callback).Run(std::move(credential_metadata));
}

void LocalCredentialManagementMac::Delete(
    Profile* profile,
    base::span<const uint8_t> credential_id,
    base::OnceCallback<void(bool)> callback) {
  NOTIMPLEMENTED();
}
