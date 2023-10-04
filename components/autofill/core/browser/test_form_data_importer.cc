// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_form_data_importer.h"

#include "build/build_config.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"

namespace autofill {

TestFormDataImporter::TestFormDataImporter(
    AutofillClient* client,
    payments::PaymentsClient* payments_client,
    std::unique_ptr<CreditCardSaveManager> credit_card_save_manager,
    std::unique_ptr<IbanSaveManager> iban_save_manager,
    PersonalDataManager* personal_data_manager,
    const std::string& app_locale,
    std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager)
    : FormDataImporter(client,
                       payments_client,
                       personal_data_manager,
                       app_locale) {
  set_credit_card_save_manager_for_testing(std::move(credit_card_save_manager));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  set_iban_save_manager_for_testing(std::move(iban_save_manager));
  set_local_card_migration_manager_for_testing(
      std::move(local_card_migration_manager));
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

}  // namespace autofill
