// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_WALLET_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_WALLET_UTILS_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/consent_auditor/consent_auditor.h"

namespace autofill {

class EntityInstance;
class EntityDataManager;

// Reacts to the response of a Wallet upsert request by writing to EDM and
// updating the UI.
//
// This is a stand-alone method and not a method of `AutofillAiManager` to allow
// running it even after the `AutofillAiManager` has been destroyed (e.g.,
// during tab close).
// The arguments have the following meaning:
// - `prompt_type`: The type of prompt that triggered the Wallet request.
// - `entity`: The entity that we tried to save/update/migrate to Wallet. Note
//    that this entity must have the same guid() as the local entity when this
//    is a migration.
// - `wallet_response`: The response from Wallet.
void HandleWalletUpsertResponse(
    base::WeakPtr<EntityDataManager> entity_manager,
    base::WeakPtr<AutofillClient> client,
    AutofillClient::AutofillAiImportPromptType prompt_type,
    EntityInstance entity,
    std::optional<EntityInstance> wallet_response);

// Returns the URL of a Wallet `entity`'s management page on wallet.google.com.
std::string GetWalletManagementURL(const EntityInstance& entity);

// Logs a `sync_pb::UserConsentTypes::WalletPrivatePassConsent` with
// `consent_string_id` and `clicked_button_string_id` as its consent details to
// the `consent_auditor` .
// This is required when saving a new Wallet private pass, either through
// settings or on import. Returns the session ID identifying the consent logged.
consent_auditor::ConsentAuditor::SessionId RecordWalletPrivatePassConsent(
    int consent_string_id,
    int clicked_button_string_id,
    consent_auditor::ConsentAuditor& consent_auditor,
    signin::IdentityManager& identity_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_WALLET_UTILS_H_
