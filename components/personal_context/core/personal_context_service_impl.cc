// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_service_impl.h"

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "components/personal_context/core/network/personal_context_manager.h"
#include "components/personal_context/core/personal_context_key_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace personal_context {

PersonalContextServiceImpl::PersonalContextServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service)
    : personal_context_manager_(std::make_unique<PersonalContextManager>(
          std::move(url_loader_factory),
          identity_manager)),
      key_manager_(pref_service ? std::make_unique<PersonalContextKeyManager>(
                                      pref_service)
                                : nullptr) {}

PersonalContextServiceImpl::~PersonalContextServiceImpl() = default;

void PersonalContextServiceImpl::Shutdown() {
  personal_context_manager_->Shutdown();
}

void PersonalContextServiceImpl::FetchContext(
    proto::ContextMemoryFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    const ContextMemoryRequestOptions& options,
    FetchContextCallback callback) {
  personal_context_manager_->FetchContext(
      feature, request_metadata, options.request_timeout, std::move(callback));
}

void PersonalContextServiceImpl::FetchPiiEntities(
    const proto::FetchPiiEntitiesRequest& request,
    const ContextMemoryRequestOptions& options,
    FetchPiiContextCallback callback) {
  personal_context_manager_->FetchPiiEntities(request, options.request_timeout,
                                              std::move(callback));
}

std::optional<proto::DecryptedEntity> PersonalContextServiceImpl::DecryptEntity(
    const proto::Entity& entity) {
  CHECK(entity.entity_case() == proto::Entity::kEncryptedEntity);

  if (!key_manager_ || entity.encrypted_entity().empty()) {
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> decrypted_bytes =
      key_manager_->Open(base::as_byte_span(entity.encrypted_entity()));
  if (!decrypted_bytes.has_value()) {
    return std::nullopt;
  }

  proto::DecryptedEntity decrypted_entity;
  if (!decrypted_entity.ParseFromArray(decrypted_bytes->data(),
                                       decrypted_bytes->size())) {
    return std::nullopt;
  }

  return decrypted_entity;
}

}  // namespace personal_context
