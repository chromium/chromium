// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_wallet_utils.h"

#include <optional>
#include <utility>

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "components/wallet/core/common/wallet_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// The URL to the overview of all passes stored in Google Wallet.
// TODO(crbug.com/454899556): Remove once Wallet deep links are launched.
constexpr char kWalletPassesPageURL[] =
    "https://wallet.google.com/wallet/passes";

// The URL to the management page of a specific private pass stored in Google
// Wallet. The pass is identified by its pass ID, which needs to be URL encoded
// into the placeholder in this URL.
constexpr char kWalletPrivatePassPageURL[] =
    "https://wallet.google.com/"
    "wallet?p=walletpass&ppid=%s&utm_source=chrome&utm_medium=settings&utm_"
    "campaign=enhanced_autofill";

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
      client->ShowAutofillAiSaveToWalletFailureNotification();
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

std::string GetWalletManagementURL(const EntityInstance& entity) {
  CHECK_EQ(entity.record_type(), EntityInstance::RecordType::kServerWallet);
  bool is_private_pass =
      IsMaskedStorageSupported(entity.type(), entity.record_type());
  // TODO(crbug.com/454899556): Implement a deep link for public passes. This is
  // not supported by the backend yet.
  if (!is_private_pass) {
    return kWalletPassesPageURL;
  }
  // Only deep link for private passes if the corresponding feature is enabled.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAiWalletPrivatePassesDeepLink)) {
    return kWalletPassesPageURL;
  }
  return base::StringPrintf(
      kWalletPrivatePassPageURL,
      base::EscapeQueryParamValue(entity.guid().value(), /*use_plus=*/false));
}

consent_auditor::ConsentAuditor::SessionId RecordWalletPrivatePassConsent(
    int accepted_consent_string_id,
    int accept_button_string_id,
    consent_auditor::ConsentAuditor& consent_auditor,
    signin::IdentityManager& identity_manager) {
  CHECK(base::FeatureList::IsEnabled(
      wallet::features::kWalletApiPrivatePassesConsent));

  // Since saves to Wallet are only offered to signed-in users, a `gaia_id` is
  // available.
  GaiaId gaia_id =
      identity_manager.GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  CHECK(!gaia_id.empty());
  consent_auditor::ConsentAuditor::SessionId session_id =
      consent_auditor.GenerateSessionId();
  sync_pb::UserConsentTypes::WalletPrivatePassConsent consent;
  consent.mutable_description_grd_ids()->Add(accepted_consent_string_id);
  consent.set_confirmation_grd_id(accept_button_string_id);
  consent_auditor.RecordWalletPrivatePassConsent(gaia_id, session_id, consent);
  return session_id;
}

}  // namespace autofill
