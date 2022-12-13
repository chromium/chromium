// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_DATA_IMPORTER_H_

#include <memory>
#include <utility>

#include "components/autofill/core/browser/form_data_importer.h"

namespace autofill {

class TestFormDataImporter : public FormDataImporter {
 public:
  TestFormDataImporter(
      AutofillClient* client,
      payments::PaymentsClient* payments_client,
      std::unique_ptr<CreditCardSaveManager> credit_card_save_manager,
      std::unique_ptr<IBANSaveManager> iban_save_manager,
      PersonalDataManager* personal_data_manager,
      const std::string& app_locale,
      std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager =
          nullptr);
  ~TestFormDataImporter() override = default;

  absl::optional<int64_t> fetched_card_instrument_id() {
    return fetched_card_instrument_id_;
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_DATA_IMPORTER_H_
