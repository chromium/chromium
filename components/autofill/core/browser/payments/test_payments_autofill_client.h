// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::payments {

// This class is for easier writing of tests. It is owned by TestAutofillClient.
class TestPaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  TestPaymentsAutofillClient();
  TestPaymentsAutofillClient(const TestPaymentsAutofillClient&) = delete;
  TestPaymentsAutofillClient& operator=(const TestPaymentsAutofillClient&) =
      delete;
  ~TestPaymentsAutofillClient() override;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_
