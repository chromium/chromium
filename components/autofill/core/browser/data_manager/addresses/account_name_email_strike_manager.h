// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STRIKE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STRIKE_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"

namespace autofill {

// This class is responsible for reacting to suggestions that are offered and
// filled with `kAccountNameEmail` profiles. On destruction (i.e. navigation)
// it will record prefs related to the ussage of those suggestions (if the
// suggestion with kAccountNameEmail profile was showed and whether it was
// filled). Owned by the `BrowserAutofillManger`, destroyed and recreated on
// navigation.
class AccountNameEmailStrikeManager : AutofillManager::Observer {
 public:
  explicit AccountNameEmailStrikeManager(AutofillManager& autofill_manager);
  ~AccountNameEmailStrikeManager() override;

  // AutofillManager::Observer:
  // Sets `was_name_email_suggestion_shown_` if any of the showed suggesions
  // contained kAccountNameEmail profile.
  void OnSuggestionsShown(
      autofill::AutofillManager& manager,
      base::span<const autofill::Suggestion> suggestions) override;
  // Checks if the kAccountNameEmail profile was filled.
  void OnFillOrPreviewForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      mojom::ActionPersistence action_persistence,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const FillingPayload& filling_payload) override;

 private:
  friend class AccountNameEmailStrikeManagerTestApi;

  base::raw_ref<AutofillClient> client_;

  bool was_name_email_profile_suggestion_shown_ = false;
  bool was_name_email_profile_filled_ = false;

  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STRIKE_MANAGER_H_
