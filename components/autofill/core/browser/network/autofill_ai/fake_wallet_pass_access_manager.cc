// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/fake_wallet_pass_access_manager.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
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
            return weakSelf->RunUpsertCallback(std::move(entity),
                                               /*is_save=*/true);
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
            return weakSelf->RunUpsertCallback(std::move(entity),
                                               /*is_save=*/false);
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
    EntityInstance entity,
    bool is_save) {
  if (features::debug::kFakeWalletApiResponsesSimulateFailure.Get()) {
    return std::nullopt;
  }

  // If saving a new entity, assign a random GUID.
  if (is_save) {
    entity = entity.CopyWithNewEntityId(EntityInstance::EntityId(
        base::Uuid::GenerateRandomV4().AsLowercaseString()));
  }

  // TODO(crbug.com/478783796): Should return a masked entity.
  return entity;
}

std::optional<EntityInstance>
FakeWalletPassAccessManager::RunGetUnmaskedCallback(
    EntityInstance::EntityId entity_id) {
  if (features::debug::kFakeWalletApiResponsesSimulateFailure.Get()) {
    return std::nullopt;
  }

  base::optional_ref<const EntityInstance> masked_entity =
      data_manager_->GetEntityInstance(entity_id);

  if (!masked_entity) {
    return std::nullopt;
  }

  EntityInstance unmasked_entity = *masked_entity;
  // TODO(crbug.com/478783796): Unmask attributes.
  return unmasked_entity;
}

}  // namespace autofill
