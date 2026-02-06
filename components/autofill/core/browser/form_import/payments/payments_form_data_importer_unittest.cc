// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"

#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class PaymentsFormDataImporterTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient> {
 public:
  void SetUp() override { InitAutofillClient(); }

  void TearDown() override { DestroyAutofillClient(); }

  PaymentsFormDataImporter& GetPaymentsFormDataImporter() {
    return autofill_client()
        .GetFormDataImporter()
        ->GetPaymentsFormDataImporter();
  }
};

}  // namespace autofill::payments
