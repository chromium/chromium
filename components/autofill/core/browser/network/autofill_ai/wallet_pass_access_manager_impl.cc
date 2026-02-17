// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/network/autofill_ai/private_pass_conversion_util.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"

namespace autofill {

namespace {

using ::wallet::PrivatePass;
using WalletRequestError = ::wallet::WalletHttpClient::WalletRequestError;

// Attempts to extract the pass number from the `response` and constructs an
// `AttributeInstance` of the corresponding `AttributeType`.
std::optional<AttributeInstance> PassNumberFromResponse(
    const PrivatePass& response) {
  auto make_attribute = [](AttributeTypeName type, std::string_view value) {
    AttributeInstance attribute((AttributeType(type)));
    // The verification status is irrelevant for unstructured data like pass
    // numbers.
    attribute.SetRawInfo(attribute.type().field_type(),
                         base::UTF8ToUTF16(value),
                         VerificationStatus::kNoStatus);
    attribute.FinalizeInfo();
    return attribute;
  };
  using enum AttributeTypeName;
  switch (response.data_case()) {
    case PrivatePass::kDriverLicense:
      return make_attribute(kDriversLicenseNumber,
                            response.driver_license().driver_license_number());
    case PrivatePass::kPassport:
      return make_attribute(kPassportNumber,
                            response.passport().passport_number());
    case PrivatePass::kIdCard:
      return make_attribute(kNationalIdCardNumber,
                            response.id_card().id_number());
    case PrivatePass::kRedressNumber:
      return make_attribute(kRedressNumberNumber,
                            response.redress_number().redress_number());
    case PrivatePass::kKnownTravelerNumber:
      return make_attribute(
          kKnownTravelerNumberNumber,
          response.known_traveler_number().known_traveler_number());
    case PrivatePass::DATA_NOT_SET:
      // Since the `response` is received from the network, it might be
      // malformed.
      return std::nullopt;
  }
  return std::nullopt;
}

bool AttributeCorrespondsToEntity(const AttributeInstance& attribute,
                                  const EntityInstance& entity) {
  return attribute.type().entity_type() == entity.type() &&
         entity.attribute(attribute.type()).has_value();
}

}  // namespace

WalletPassAccessManagerImpl::WalletPassAccessManagerImpl(
    std::unique_ptr<wallet::WalletHttpClient> http_client,
    EntityDataManager* data_manager)
    : http_client_(std::move(http_client)),
      data_manager_(CHECK_DEREF(data_manager)) {
  data_manager_observer_.Observe(&data_manager_.get());
}

WalletPassAccessManagerImpl::~WalletPassAccessManagerImpl() = default;

void WalletPassAccessManagerImpl::SaveWalletEntityInstance(
    const EntityInstance& entity,
    UpsertEntityInstanceCallback callback) {
  PrivatePass pass = EntityInstanceToPrivatePass(entity);
  // To indicate saving of a new entity, the pass ID in the request is kept
  // empty. The server-side will choose an identifier and return it through the
  // Upsert response, which `UpsertPrivatePass()` uses to set the result's ID.
  pass.clear_pass_id();
  http_client_->UpsertPrivatePass(
      std::move(pass), GetUpsertResponseToMaskedEntityCallback(entity).Then(
                           std::move(callback)));
}

void WalletPassAccessManagerImpl::UpdateWalletEntityInstance(
    const EntityInstance& entity,
    UpsertEntityInstanceCallback callback) {
  PrivatePass pass = EntityInstanceToPrivatePass(entity);
  // To indicate updating of an existing entity, the request has a pass ID set.
  // The Upsert response and thus the entity returned through the `callback`
  // might still contain a different ID, meaning that deduplication happened on
  // the server-side.
  CHECK(pass.has_pass_id());
  http_client_->UpsertPrivatePass(
      std::move(pass), GetUpsertResponseToMaskedEntityCallback(entity).Then(
                           std::move(callback)));
}

void WalletPassAccessManagerImpl::GetUnmaskedWalletEntityInstance(
    const EntityInstance::EntityId& entity_id,
    GetUnmaskedEntityInstanceCallback callback) {
  if (auto it = unmasked_entity_cache_.find(entity_id);
      it != unmasked_entity_cache_.end()) {
    std::move(callback).Run(it->second);
    return;
  }
  base::optional_ref<const EntityInstance> masked_entity =
      data_manager_->GetEntityInstance(entity_id);
  if (!masked_entity) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  CHECK(masked_entity->IsMaskedServerEntity());
  auto maybe_cache_response = base::BindOnce(
      [](base::WeakPtr<WalletPassAccessManagerImpl> access_manager,
         std::optional<EntityInstance> entity) {
        if (access_manager && entity) {
          access_manager->CacheUnmaskResult(*entity);
        }
        return entity;
      },
      weak_factory_.GetWeakPtr());
  http_client_->GetUnmaskedPass(
      entity_id.value(),
      GetUnmaskResponseToUnmaskedEntityCallback(*masked_entity)
          .Then(std::move(maybe_cache_response))
          .Then(std::move(callback)));
}

base::OnceCallback<std::optional<EntityInstance>(
    const base::expected<PrivatePass, WalletRequestError>&)>
WalletPassAccessManagerImpl::GetUnmaskResponseToUnmaskedEntityCallback(
    const EntityInstance& masked_entity) const {
  return base::BindOnce(
      [](EntityInstance masked_entity,
         const base::expected<PrivatePass, WalletRequestError>& response)
          -> std::optional<EntityInstance> {
        if (!response.has_value()) {
          return std::nullopt;
        }
        std::optional<AttributeInstance> unmasked_pass_number =
            PassNumberFromResponse(response.value());
        // Make sure the response type corresponds to the entity.
        if (!unmasked_pass_number.has_value() ||
            !AttributeCorrespondsToEntity(*unmasked_pass_number,
                                          masked_entity)) {
          return std::nullopt;
        }
        CHECK(!unmasked_pass_number->masked());
        EntityInstance unmasked_entity = masked_entity.CopyWithUpdatedAttribute(
            std::move(*unmasked_pass_number));
        CHECK(unmasked_entity.IsUnmaskedServerEntity());
        return unmasked_entity;
      },
      masked_entity);
}

base::OnceCallback<std::optional<EntityInstance>(
    const base::expected<PrivatePass, WalletRequestError>&)>
WalletPassAccessManagerImpl::GetUpsertResponseToMaskedEntityCallback(
    const EntityInstance& unmasked_entity) const {
  CHECK(unmasked_entity.IsUnmaskedServerEntity());
  return base::BindOnce(
      [](EntityInstance unmasked_entity,
         const base::expected<PrivatePass, WalletRequestError>& response)
          -> std::optional<EntityInstance> {
        if (!response.has_value()) {
          return std::nullopt;
        }
        std::optional<AttributeInstance> masked_pass_number =
            PassNumberFromResponse(response.value());
        if (!masked_pass_number.has_value() ||
            !AttributeCorrespondsToEntity(*masked_pass_number,
                                          unmasked_entity)) {
          return std::nullopt;
        }
        masked_pass_number->mark_as_masked({});
        EntityInstance masked_entity =
            unmasked_entity
                .CopyWithNewEntityId(
                    EntityInstance::EntityId(response->pass_id()))
                .CopyWithUpdatedAttribute(std::move(*masked_pass_number));
        CHECK(masked_entity.IsMaskedServerEntity());
        return masked_entity;
      },
      unmasked_entity);
}

void WalletPassAccessManagerImpl::CacheUnmaskResult(EntityInstance entity) {
  EntityInstance::EntityId id = entity.guid();
  auto [it, inserted] = unmasked_entity_cache_.insert({id, std::move(entity)});
  if (!inserted) {
    return;
  }
  // Clear the cache entry after `kCacheTTL`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<WalletPassAccessManagerImpl> access_manager,
             const EntityInstance::EntityId& id) {
            if (!access_manager) {
              return;
            }
            auto& cache = access_manager->unmasked_entity_cache_;
            if (auto it = cache.find(id); it != cache.end()) {
              cache.erase(it);
            }
          },
          weak_factory_.GetWeakPtr(), id),
      kCacheTTL);
}

void WalletPassAccessManagerImpl::OnEntityInstancesChanged() {
  // `OnEntityInstancesChanged()` doesn't indicate what has changed exactly,
  // since multiple entities can change at once.
  // Conservatively remove all cache entries that either:
  // - Don't have a corresponding entity in the data manager.
  // - Differ from their data manager representation.
  absl::erase_if(
      unmasked_entity_cache_,
      [&](const std::pair<EntityInstance::EntityId, EntityInstance>& entry) {
        const auto& [id, cached_entity] = entry;
        base::optional_ref<const EntityInstance> entity =
            data_manager_->GetEntityInstance(id);
        if (!entity) {
          return true;
        }
        // Erase `cache_entry` unless it matches `*entity`. Note that this
        // doesn't use `EntityInstance::operator==()`, since operator== doesn't
        // take masked attributes into consideration, while for `IsSubsetOf()`,
        // comparison is done using the unmasked attribute's suffix.
        return !entity->IsSubsetOf(cached_entity) ||
               !cached_entity.IsSubsetOf(*entity);
      });
}

}  // namespace autofill
