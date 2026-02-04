// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_WALLET_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_WALLET_UTILS_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

class EntityInstance;
class EntityManager;

// Reacts to the response of a Wallet upsert request by writing to EDM and
// updating the UI.
//
// This is a stand-alone method and not a method of `AutofillAiManager` to allow
// running it even after the `AutofillAiManager` has been destroyed (e.g.,
// during tab close).
void HandleWalletUpsertResponse(
    base::WeakPtr<EntityManager> entity_manager,
    base::WeakPtr<AutofillClient> client,
    AutofillClient::AutofillAiImportPromptType prompt_type,
    std::optional<EntityInstance> wallet_response);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_WALLET_UTILS_H_
