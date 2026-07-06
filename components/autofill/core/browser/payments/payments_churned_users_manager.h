// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"

namespace autofill::payments {

// Owned by PaymentsAutofillClient. There is one instance of this class per
// WebContents. This class handles flows related to bringing back users that
// have payments autofill turned off.
class PaymentsChurnedUsersManager : public AutofillManager::Observer {
 public:
  explicit PaymentsChurnedUsersManager(AutofillClient* autofill_client);
  PaymentsChurnedUsersManager(const PaymentsChurnedUsersManager& other) =
      delete;
  PaymentsChurnedUsersManager& operator=(
      const PaymentsChurnedUsersManager& other) = delete;
  ~PaymentsChurnedUsersManager() override;

  // AutofillManager::Observer:
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              AutofillManager::Observer::FieldTypeSource source,
                              bool small_forms_were_parsed) override;

 private:
  ScopedAutofillManagersObservation autofill_managers_observation_{this};

  base::WeakPtrFactory<PaymentsChurnedUsersManager> weak_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_MANAGER_H_
