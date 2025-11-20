// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/unexportable_keys/mojom/unexportable_key_service_proxy_impl.h"

#include "base/check_deref.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom-data-view.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace unexportable_keys {

namespace {

base::expected<mojom::NewKeyDataPtr, ServiceError> PopulateNewKeyData(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    const ServiceErrorOr<UnexportableKeyId> error_or_key_id) {
  if (!error_or_key_id.has_value()) {
    return base::unexpected(error_or_key_id.error());
  }

  const UnexportableKeyId& key_id = error_or_key_id.value();
  auto new_key_data = mojom::NewKeyData::New();

  new_key_data->key_id = key_id;

  const ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm> algo =
      unexportable_key_service.GetAlgorithm(key_id);
  if (!algo.has_value()) {
    return base::unexpected(algo.error());
  }
  new_key_data->algorithm = *algo;

  const ServiceErrorOr<std::vector<uint8_t>> wrapped =
      unexportable_key_service.GetWrappedKey(key_id);
  if (!wrapped.has_value()) {
    return base::unexpected(wrapped.error());
  }
  new_key_data->wrapped_key = *wrapped;

  const ServiceErrorOr<std::vector<uint8_t>> key_info =
      unexportable_key_service.GetSubjectPublicKeyInfo(key_id);
  if (!key_info.has_value()) {
    return base::unexpected(key_info.error());
  }
  new_key_data->subject_public_key_info = *key_info;

  return new_key_data;
}
}  // namespace

UnexportableKeyServiceProxyImpl::UnexportableKeyServiceProxyImpl(
    unexportable_keys::UnexportableKeyService* unexportable_key_service,
    mojo::PendingReceiver<mojom::UnexportableKeyService> receiver)
    : receiver_(this, std::move(receiver)),
      unexportable_key_service_(CHECK_DEREF(unexportable_key_service)) {}

UnexportableKeyServiceProxyImpl::~UnexportableKeyServiceProxyImpl() = default;

void unexportable_keys::UnexportableKeyServiceProxyImpl::GenerateSigningKey(
    const std::vector<crypto::SignatureVerifier::SignatureAlgorithm>&
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    GenerateSigningKeyCallback callback) {
  unexportable_key_service_->GenerateSigningKeySlowlyAsync(
      acceptable_algorithms, priority,
      base::BindOnce(PopulateNewKeyData, std::ref(*unexportable_key_service_))
          .Then(std::move(callback)));
}

void unexportable_keys::UnexportableKeyServiceProxyImpl::FromWrappedSigningKey(
    const std::vector<uint8_t>& wrapped_key,
    BackgroundTaskPriority priority,
    FromWrappedSigningKeyCallback callback) {
  if (wrapped_key.size() > kMaxWrappedKeySize) {
    receiver_.ReportBadMessage("wrapped key size too long");
    return;
  }
  unexportable_key_service_->FromWrappedSigningKeySlowlyAsync(
      wrapped_key, priority,
      base::BindOnce(PopulateNewKeyData, std::ref(*unexportable_key_service_))
          .Then(std::move(callback)));
}
}  // namespace unexportable_keys
