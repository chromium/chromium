// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/strike_databases/payments/payments_churned_users_strike_database.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"

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

  PaymentsChurnedUsersStrikeDatabase* GetStrikeDatabaseForTesting() {
    return strike_database_.get();
  }

 private:
  void OnUiClosed(PaymentsUiClosedReason closed_reason);

  // The associated AutofillClient.
  const raw_ref<AutofillClient> client_;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};

  // Strike database used to ensure the payments churned users UI is shown a
  // designated number of times, with delays in between shows.
  std::unique_ptr<PaymentsChurnedUsersStrikeDatabase> strike_database_;

  base::WeakPtrFactory<PaymentsChurnedUsersManager> weak_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_MANAGER_H_
