// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_service_impl.h"

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/personal_context/core/network/personal_context_manager.h"
#include "components/personal_context/core/personal_context_key_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace personal_context {

namespace {

void RecordDecryptionMetrics(PersonalContextDecryptionStatus status,
                             base::TimeDelta latency) {
  base::UmaHistogramEnumeration("PersonalContext.DecryptEntity.Status", status);
  base::UmaHistogramBoolean("PersonalContext.DecryptEntity.Result",
                            status == PersonalContextDecryptionStatus::kSuccess);
  base::UmaHistogramMicrosecondsTimes("PersonalContext.DecryptEntity.Latency",
                                      latency);
}

}  // namespace

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
  const base::TimeTicks start_time = base::TimeTicks::Now();

  if (!key_manager_) {
    RecordDecryptionMetrics(PersonalContextDecryptionStatus::kNoKeyManager,
                            base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  if (entity.encrypted_entity().empty()) {
    RecordDecryptionMetrics(
        PersonalContextDecryptionStatus::kEmptyEncryptedEntity,
        base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> decrypted_bytes =
      key_manager_->Open(base::as_byte_span(entity.encrypted_entity()));
  if (!decrypted_bytes.has_value()) {
    RecordDecryptionMetrics(PersonalContextDecryptionStatus::kDecryptionFailed,
                            base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  proto::DecryptedEntity decrypted_entity;
  if (!decrypted_entity.ParseFromArray(decrypted_bytes->data(),
                                       decrypted_bytes->size())) {
    RecordDecryptionMetrics(PersonalContextDecryptionStatus::kProtoParseFailed,
                            base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  RecordDecryptionMetrics(PersonalContextDecryptionStatus::kSuccess,
                          base::TimeTicks::Now() - start_time);
  return decrypted_entity;
}

}  // namespace personal_context
