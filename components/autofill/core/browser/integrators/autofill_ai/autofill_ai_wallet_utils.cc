// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_wallet_utils.h"

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

namespace {

// Defines UI actions that can be taken after a Wallet upsert response.
enum class UiAction {
  kLocalSaveNotification,
  kUpdateOrMigrateFailureNotification,
  kNoNotification
};

// Hides the import bubble and performs the UI update specified by `action`.
void UpdateUi(base::WeakPtr<AutofillClient> client, UiAction action) {
  if (!client) {
    return;
  }
  client->CloseEntityImportBubble();
  switch (action) {
    case UiAction::kLocalSaveNotification:
      client->ShowAutofillAiLocalSaveNotification();
      break;
    case UiAction::kUpdateOrMigrateFailureNotification:
      // TODO(crbug.com/477845712): Implement.
      break;
    case UiAction::kNoNotification:
      break;
  }
}

}  // namespace

void HandleWalletUpsertResponse(
    base::WeakPtr<EntityDataManager> entity_manager,
    base::WeakPtr<AutofillClient> client,
    AutofillClient::AutofillAiImportPromptType prompt_type,
    EntityInstance entity,
    std::optional<EntityInstance> wallet_response) {
  using enum AutofillClient::AutofillAiImportPromptType;
  using enum UiAction;

  CHECK(entity.type().SupportsMaskedStorage());
  CHECK(!entity.IsMaskedServerEntity());

  if (!entity_manager) {
    UpdateUi(client, kNoNotification);
    return;
  }

  // The Wallet request failed.
  if (!wallet_response) {
    switch (prompt_type) {
      case kSave:
        // Save locally instead.
        entity_manager->AddOrUpdateEntityInstance(
            entity.CopyWithNewRecordType(EntityInstance::RecordType::kLocal));
        UpdateUi(client, kLocalSaveNotification);
        break;
      case kUpdate:
        UpdateUi(client, kUpdateOrMigrateFailureNotification);
        break;
      case kMigrate:
        UpdateUi(client, kUpdateOrMigrateFailureNotification);
        break;
    }
    return;
  }

  // The Wallet server API must always return a masked entity. This CHECK can be
  // enforced on the client even though it involves server data because the bit
  // whether an attribute is masked is purely determined client-side.
  CHECK(!wallet_response->IsUnmaskedServerEntity());
  switch (prompt_type) {
    case kMigrate:
      // TODO(crbug.com/481566741): Delete local entity.
      [[fallthrough]];
    case kSave:
    case kUpdate:
      // TODO(crbug.com/481566741): Write the entity to EDM.
      break;
  }
  UpdateUi(client, kNoNotification);
}

}  // namespace autofill
