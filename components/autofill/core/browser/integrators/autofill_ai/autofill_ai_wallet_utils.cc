// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_wallet_utils.h"

#include <optional>
#include <utility>

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
      client->ShowAutofillAiFailureNotification(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_WALLET_UPDATE_OR_MIGRATE_FAILURE_NOTIFICATION));
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

  CHECK(IsMaskedStorageSupported(entity.type(), entity.record_type()));
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
      entity_manager->RemoveEntityInstance(entity.guid());
      [[fallthrough]];
    case kSave:
    case kUpdate:
      entity_manager->AddOrUpdateEntityInstance(std::move(*wallet_response));
      break;
  }
  UpdateUi(client, kNoNotification);
}

}  // namespace autofill
