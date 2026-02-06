// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_TEST_API_H_

#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"

namespace autofill::payments {

class PaymentsFormDataImporterTestApi {
 public:
  explicit PaymentsFormDataImporterTestApi(
      PaymentsFormDataImporter* payments_fdi)
      : payments_fdi_(*payments_fdi) {}

 private:
  const raw_ref<PaymentsFormDataImporter> payments_fdi_;
};

inline PaymentsFormDataImporterTestApi test_api(
    PaymentsFormDataImporter& payments_fdi) {
  return PaymentsFormDataImporterTestApi(&payments_fdi);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_TEST_API_H_
