// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_form_data_importer.h"

#include "build/build_config.h"
#include "components/autofill/core/browser/form_data_importer_test_api.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"

namespace autofill {

TestFormDataImporter::TestFormDataImporter(
    AutofillClient* client,
    std::unique_ptr<CreditCardSaveManager> credit_card_save_manager,
    std::unique_ptr<IbanSaveManager> iban_save_manager,
    const std::string& app_locale,
    std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager)
    : FormDataImporter(client,
                       /*history_service=*/nullptr,
                       app_locale) {
  test_api(*this).set_credit_card_save_manager(
      std::move(credit_card_save_manager));
  test_api(*this).set_iban_save_manager(std::move(iban_save_manager));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  test_api(*this).set_local_card_migration_manager(
      std::move(local_card_migration_manager));
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

}  // namespace autofill
