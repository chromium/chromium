// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai_delegate.h"
#include "components/autofill/core/common/unique_ids.h"

namespace optimization_guide::proto {
class AXTreeUpdate;
}

namespace autofill_ai {

class AutofillAiModelExecutor;
class AutofillAiManager;

// An interface for embedder actions, e.g. Chrome on Desktop.
//
// A client should be created only if
// `AutofillAiIsPlatformAndEnterprisePolicyEligible()`. However,
// `AutofillAiIsPlatformAndEnterprisePolicyEligible()` is not necessarily a
// constant over the lifetime of the client. For example, the user may disable
// Autofill in the settings while the client is alive.
class AutofillAiClient {
 public:
  // Contains the result of a user interaction with the save/update AutofillAi
  // prompt.
  struct SaveOrUpdatePromptResult final {
    SaveOrUpdatePromptResult();
    SaveOrUpdatePromptResult(bool did_user_decline,
                             std::optional<autofill::EntityInstance> entity);
    SaveOrUpdatePromptResult(const SaveOrUpdatePromptResult&);
    SaveOrUpdatePromptResult(SaveOrUpdatePromptResult&&);
    SaveOrUpdatePromptResult& operator=(const SaveOrUpdatePromptResult&);
    SaveOrUpdatePromptResult& operator=(SaveOrUpdatePromptResult&&);
    ~SaveOrUpdatePromptResult();

    // Whether the user explicitly declined the dialog.
    bool did_user_decline = false;

    // Non-empty iff the prompt was accepted.
    std::optional<autofill::EntityInstance> entity;
  };
  using SaveOrUpdatePromptResultCallback =
      base::OnceCallback<void(SaveOrUpdatePromptResult result)>;

  // The callback to extract the accessibility tree snapshot.
  using AXTreeCallback =
      base::OnceCallback<void(optimization_guide::proto::AXTreeUpdate)>;

  virtual ~AutofillAiClient() = default;

  // Returns the AutofillClient that is scoped to the same object (e.g., tab) as
  // this AutofillAiClient.
  virtual autofill::AutofillClient& GetAutofillClient() = 0;
  const autofill::AutofillClient& GetAutofillClient() const {
    return const_cast<const autofill::AutofillClient&>(
        const_cast<AutofillAiClient*>(this)->GetAutofillClient());
  }

  // Calls `callback` with the accessibility tree snapshot.
  virtual void GetAXTree(AXTreeCallback callback) = 0;

  // Returns the `AutofillAiManager` associated with this
  // client.
  virtual AutofillAiManager& GetManager() = 0;

  // Returns a pointer to the current profile's `autofill::EntityDataManager`.
  // Can be `nullptr` if `features::kAutofillAiWithDataSchema` is disabled.
  virtual autofill::EntityDataManager* GetEntityDataManager() = 0;

  // Returns whether the feature is enabled in the prefs
  // (`autofill::prefs::kAutofillAisEnabled`).
  //
  // This is different from `AutofillAiIsPlatformAndEnterprisePolicyEligible()`,
  // which checks if the platform and enterprise policy allow the feature (if
  // not, the client is not instantiated in the first place).
  virtual bool IsAutofillAiEnabledPref() const = 0;

  // Returns whether the current user is eligible for Autofill AI.
  virtual bool IsUserEligible() = 0;

  // Returns a pointer to a `FormStructure` for the corresponding `form_id`
  // from the Autofill cache. Can be a `nullptr` when the structure was not
  // found or if the driver is not available.
  virtual autofill::FormStructure* GetCachedFormStructure(
      const autofill::FormGlobalId& form_id) = 0;

  // Shows a bubble asking whether the user wants to save or update Autofill AI
  // data. `old_entity` is present in the update cases. It is used to give users
  // a better understanding of what was updated.
  virtual void ShowSaveOrUpdateBubble(
      autofill::EntityInstance new_entity,
      std::optional<autofill::EntityInstance> old_entity,
      SaveOrUpdatePromptResultCallback save_prompt_acceptance_callback) = 0;
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_
