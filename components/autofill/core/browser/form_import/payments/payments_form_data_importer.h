// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_H_

#include "base/memory/raw_ref.h"

namespace autofill {

class AutofillClient;

namespace payments {

// Owned by `FormDataImporter`. Responsible for payments-related form data
// importing functionality, usually on submission. Some examples of such
// functionality includes form extraction and processing, feature enrollment,
// and autofill table updating.
class PaymentsFormDataImporter {
 public:
  explicit PaymentsFormDataImporter(AutofillClient* client);
  PaymentsFormDataImporter(const PaymentsFormDataImporter&) = delete;
  PaymentsFormDataImporter& operator=(const PaymentsFormDataImporter&) = delete;
  virtual ~PaymentsFormDataImporter();

 private:
  const raw_ref<AutofillClient> client_;
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_H_
