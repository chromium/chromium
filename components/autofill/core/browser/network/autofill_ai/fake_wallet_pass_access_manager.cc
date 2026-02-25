// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/fake_wallet_pass_access_manager.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/common/autofill_debug_features.h"

namespace autofill {

FakeWalletPassAccessManager::FakeWalletPassAccessManager(
    EntityDataManager* data_manager)
    : data_manager_(CHECK_DEREF(data_manager)) {}

FakeWalletPassAccessManager::~FakeWalletPassAccessManager() = default;

void FakeWalletPassAccessManager::SaveWalletEntityInstance(
    const EntityInstance& entity,
    UpsertEntityInstanceCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<FakeWalletPassAccessManager> weakSelf,
             EntityInstance entity) -> std::optional<EntityInstance> {
            if (!weakSelf) {
              return std::nullopt;
            }
            return weakSelf->RunUpsertCallback(std::move(entity));
          },
          weak_ptr_factory_.GetWeakPtr(), entity)
          .Then(std::move(callback)),
      base::Milliseconds(
          features::debug::kFakeWalletApiResponsesDelayMs.Get()));
}

void FakeWalletPassAccessManager::UpdateWalletEntityInstance(
    const EntityInstance& entity,
    UpsertEntityInstanceCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<FakeWalletPassAccessManager> weakSelf,
             EntityInstance entity) -> std::optional<EntityInstance> {
            if (!weakSelf) {
              return std::nullopt;
            }
            return weakSelf->RunUpsertCallback(std::move(entity));
          },
          weak_ptr_factory_.GetWeakPtr(), entity)
          .Then(std::move(callback)),
      base::Milliseconds(
          features::debug::kFakeWalletApiResponsesDelayMs.Get()));
}

void FakeWalletPassAccessManager::GetUnmaskedWalletEntityInstance(
    const EntityInstance::EntityId& entity_id,
    GetUnmaskedEntityInstanceCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<FakeWalletPassAccessManager> weakSelf,
             EntityInstance::EntityId id) -> std::optional<EntityInstance> {
            if (!weakSelf) {
              return std::nullopt;
            }
            return weakSelf->RunGetUnmaskedCallback(std::move(id));
          },
          weak_ptr_factory_.GetWeakPtr(), entity_id)
          .Then(std::move(callback)),
      base::Milliseconds(
          features::debug::kFakeWalletApiResponsesDelayMs.Get()));
}

std::optional<EntityInstance> FakeWalletPassAccessManager::RunUpsertCallback(
    EntityInstance entity) {
  if (features::debug::kFakeWalletApiResponsesSimulateFailure.Get()) {
    return std::nullopt;
  }

  upserted_unmasked_entities_.insert_or_assign(entity.guid(), entity);

  EntityInstance masked_entity = entity;
  for (const AttributeInstance& attr : entity.attributes()) {
    if (!attr.type().is_obfuscated()) {
      continue;
    }

    AttributeInstance masked_attr = attr;
    const FieldType field_type = masked_attr.type().field_type();
    const std::u16string full_value = masked_attr.GetInfo(
        field_type, "en-US", /*format_string=*/std::nullopt);
    const size_t masked_length = std::min<size_t>(full_value.size(), 4);
    masked_attr.SetInfo(
        masked_attr.type().field_type(),
        /*value=*/full_value.substr(full_value.size() - masked_length),
        /*app_locale=*/"en-US",
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);

    masked_attr.mark_as_masked({});

    masked_entity =
        masked_entity.CopyWithUpdatedAttribute(std::move(masked_attr));
  }

  return masked_entity;
}

std::optional<EntityInstance>
FakeWalletPassAccessManager::RunGetUnmaskedCallback(
    EntityInstance::EntityId entity_id) {
  if (features::debug::kFakeWalletApiResponsesSimulateFailure.Get()) {
    return std::nullopt;
  }

  auto it = upserted_unmasked_entities_.find(entity_id);
  if (it != upserted_unmasked_entities_.end()) {
    return it->second;
  }

  NOTIMPLEMENTED();
  return std::nullopt;
}

}  // namespace autofill
