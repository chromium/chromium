// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals.mojom.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"

namespace {

std::string_view GetAlgorithmName(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
      return "RSA_PKCS1_SHA1";
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      return "RSA_PKCS1_SHA256";
    case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      return "ECDSA_SHA256";
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
      return "RSA_PSS_SHA256";
  }
}

}  // namespace

UnexportableKeysInternalsHandler::UnexportableKeysInternalsHandler(
    mojo::PendingReceiver<unexportable_keys_internals::mojom::PageHandler>
        receiver,
    mojo::PendingRemote<unexportable_keys_internals::mojom::Page> page,
    std::unique_ptr<unexportable_keys::UnexportableKeyService> key_service)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      key_service_(std::move(key_service)) {
  CHECK(key_service_);
}

UnexportableKeysInternalsHandler::~UnexportableKeysInternalsHandler() = default;

void UnexportableKeysInternalsHandler::GetUnexportableKeysInfo(
    GetUnexportableKeysInfoCallback callback) {
  key_service_->GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::BindOnce(&UnexportableKeysInternalsHandler::
                         OnGetAllSigningKeysForGarbageCollection,
                     // `this` is guaranteed to be alive because `key_service_`
                     // is owned by `this`.
                     base::Unretained(this), std::move(callback)));
}

void UnexportableKeysInternalsHandler::DeleteKey(
    const unexportable_keys::UnexportableKeyId& key_id,
    DeleteKeyCallback callback) {
  key_service_->DeleteKeysSlowlyAsync(
      {key_id}, unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::BindOnce(
          [](DeleteKeyCallback callback,
             unexportable_keys::ServiceErrorOr<size_t> result) {
            std::move(callback).Run(static_cast<bool>(result.value_or(0)));
          },
          std::move(callback)));
}

void UnexportableKeysInternalsHandler::OnGetAllSigningKeysForGarbageCollection(
    GetUnexportableKeysInfoCallback callback,
    unexportable_keys::ServiceErrorOr<
        std::vector<unexportable_keys::UnexportableKeyId>> keys) {
  if (!keys.has_value()) {
    std::move(callback).Run({});
    return;
  }

  std::vector<unexportable_keys_internals::mojom::UnexportableKeyInfoPtr>
      key_infos;
  for (const auto& key : *keys) {
    auto key_info =
        unexportable_keys_internals::mojom::UnexportableKeyInfo::New();
    key_info->key_id = key;

    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
        key_service_->GetWrappedKey(key);
    if (!wrapped_key.has_value()) {
      continue;
    }
    key_info->wrapped_key = base::Base64Encode(*wrapped_key);

    unexportable_keys::ServiceErrorOr<
        crypto::SignatureVerifier::SignatureAlgorithm>
        algorithm = key_service_->GetAlgorithm(key);
    if (!algorithm.has_value()) {
      continue;
    }
    key_info->algorithm = GetAlgorithmName(*algorithm);

    unexportable_keys::ServiceErrorOr<std::string> key_tag =
        key_service_->GetKeyTag(key);
    if (!key_tag.has_value()) {
      continue;
    }
    key_info->key_tag = *std::move(key_tag);

    unexportable_keys::ServiceErrorOr<base::Time> creation_time =
        key_service_->GetCreationTime(key);
    if (!creation_time.has_value()) {
      continue;
    }
    key_info->creation_time = *creation_time;

    key_infos.push_back(std::move(key_info));
  }

  std::move(callback).Run(std::move(key_infos));
}
