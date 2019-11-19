// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_

#include <string>

#include "components/autofill/core/browser/payments/credit_card_save_manager.h"

namespace autofill {

namespace payments {
class TestPaymentsClient;
}  // namespace payments

class AutofillClient;
class AutofillDriver;
class PersonalDataManager;

class TestCreditCardSaveManager : public CreditCardSaveManager {
 public:
  TestCreditCardSaveManager(AutofillDriver* driver,
                            AutofillClient* client,
                            payments::TestPaymentsClient* payments_client,
                            PersonalDataManager* personal_data_manager);
  ~TestCreditCardSaveManager() override;

  bool IsCreditCardUploadEnabled() override;

  void SetCreditCardUploadEnabled(bool credit_card_upload_enabled);

  // Returns whether OnDidUploadCard() was called.
  bool CreditCardWasUploaded();

  void set_show_save_prompt(bool show_save_prompt);

  void set_upload_request_card_number(const base::string16& credit_card_number);

 private:
  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result,
                       const std::string& server_id) override;

  bool credit_card_upload_enabled_ = false;
  bool credit_card_was_uploaded_ = false;

  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveManagerTest,
                           UploadCreditCard_NumStrikesLoggedOnUploadNotSuccess);

  DISALLOW_COPY_AND_ASSIGN(TestCreditCardSaveManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_
