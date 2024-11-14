// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class AutofillClient;
class AutofillProfile;
class CreditCard;
class FormStructure;

// For each submitted field in the `form_structure`, determines whether
// `ADDRESS_HOME_STATE` is a possible matching type.
// This function is intended to run before
// DeterminePossibleFieldTypesForUpload() on the browser UI thread.
void PreProcessStateMatchingTypes(const AutofillClient& client,
                                  const std::vector<AutofillProfile>& profiles,
                                  FormStructure& form_structure);

// Uses the existing personal data in |profiles| and |credit_cards| to
// determine possible field types for the |form|.  This is
// potentially expensive -- on the order of 50ms even for a small set of
// |stored_data|. Hence, it should not run on the UI thread -- to avoid
// locking up the UI -- nor on the IO thread -- to avoid blocking IPC calls.
void DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::u16string& last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    FormStructure* form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_
