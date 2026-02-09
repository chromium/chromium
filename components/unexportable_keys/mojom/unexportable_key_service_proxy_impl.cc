// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/unexportable_keys/mojom/unexportable_key_service_proxy_impl.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace unexportable_keys {

namespace {

// For operations requiring stateful keys `ServiceError::kOperationNotSupported`
// might be returned on platforms where keys are stateless. Since this is not an
// actual error when retrieving the key, treat it simply as a missing key.
template <typename T>
ServiceErrorOr<std::optional<T>> AdaptOperationNotSupported(
    ServiceErrorOr<T> result) {
  using ServiceErrorOrOpt = ServiceErrorOr<std::optional<T>>;
  return ServiceErrorOrOpt(std::move(result)).or_else([](ServiceError error) {
    return error == ServiceError::kOperationNotSupported
               ? ServiceErrorOrOpt(std::nullopt)
               : base::unexpected(error);
  });
}

ServiceErrorOr<mojom::NewKeyDataPtr> PopulateNewKeyData(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    const ServiceErrorOr<UnexportableKeyId> error_or_key_id) {
  ASSIGN_OR_RETURN(UnexportableKeyId key_id, error_or_key_id);
  auto new_key_data = mojom::NewKeyData::New();
  new_key_data->key_id = key_id;

  ASSIGN_OR_RETURN(new_key_data->algorithm,
                   unexportable_key_service.GetAlgorithm(key_id));
  ASSIGN_OR_RETURN(new_key_data->wrapped_key,
                   unexportable_key_service.GetWrappedKey(key_id));
  ASSIGN_OR_RETURN(new_key_data->subject_public_key_info,
                   unexportable_key_service.GetSubjectPublicKeyInfo(key_id));
  ASSIGN_OR_RETURN(
      new_key_data->key_tag,
      AdaptOperationNotSupported(unexportable_key_service.GetKeyTag(key_id)));
  ASSIGN_OR_RETURN(new_key_data->creation_time,
                   AdaptOperationNotSupported(
                       unexportable_key_service.GetCreationTime(key_id)));
  return new_key_data;
}

ServiceErrorOr<uint64_t> AdaptSizeType(ServiceErrorOr<size_t> result) {
  return result.transform(
      [](size_t r) { return base::strict_cast<uint64_t>(r); });
}

ServiceErrorOr<std::vector<mojom::NewKeyDataPtr>> PopulateAllNewKeyData(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    ServiceErrorOr<std::vector<UnexportableKeyId>> error_or_key_ids) {
  ASSIGN_OR_RETURN(std::vector<UnexportableKeyId> key_ids, error_or_key_ids);
  std::vector<mojom::NewKeyDataPtr> new_key_data;
  new_key_data.reserve(key_ids.size());
  for (UnexportableKeyId key_id : key_ids) {
    ASSIGN_OR_RETURN(mojom::NewKeyDataPtr data,
                     PopulateNewKeyData(unexportable_key_service, key_id));
    new_key_data.push_back(std::move(data));
  }
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

void unexportable_keys::UnexportableKeyServiceProxyImpl::Sign(
    const UnexportableKeyId& key_id,
    const std::vector<uint8_t>& data,
    BackgroundTaskPriority priority,
    SignCallback callback) {
  unexportable_key_service_->SignSlowlyAsync(key_id, data, priority,
                                             std::move(callback));
}

void unexportable_keys::UnexportableKeyServiceProxyImpl::
    GetAllSigningKeysForGarbageCollection(
        BackgroundTaskPriority priority,
        GetAllSigningKeysForGarbageCollectionCallback callback) {
  unexportable_key_service_->GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      priority, base::BindOnce(PopulateAllNewKeyData,
                               std::ref(*unexportable_key_service_))
                    .Then(std::move(callback)));
}

void unexportable_keys::UnexportableKeyServiceProxyImpl::DeleteKeys(
    const std::vector<UnexportableKeyId>& key_ids,
    BackgroundTaskPriority priority,
    DeleteKeysCallback callback) {
  unexportable_key_service_->DeleteKeysSlowlyAsync(
      key_ids, priority,
      base::BindOnce(&AdaptSizeType).Then(std::move(callback)));
}

void unexportable_keys::UnexportableKeyServiceProxyImpl::DeleteAllKeys(
    DeleteAllKeysCallback callback) {
  unexportable_key_service_->DeleteAllKeysSlowlyAsync(
      base::BindOnce(&AdaptSizeType).Then(std::move(callback)));
}

}  // namespace unexportable_keys
