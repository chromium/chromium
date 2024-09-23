// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_

#include <optional>
#include <string>

#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

class AutofillClient;

class TestCreditCardSaveManager : public CreditCardSaveManager {
 public:
  explicit TestCreditCardSaveManager(AutofillClient* client);

  TestCreditCardSaveManager(const TestCreditCardSaveManager&) = delete;
  TestCreditCardSaveManager& operator=(const TestCreditCardSaveManager&) =
      delete;

  ~TestCreditCardSaveManager() override;

  bool IsCreditCardUploadEnabled() override;

  void SetCreditCardUploadEnabled(bool credit_card_upload_enabled);

  // Returns whether OnDidUploadCard() was called.
  bool CreditCardWasUploaded();

  // Returns whether AttemptToOfferCvcLocalSave() was called.
  bool CvcLocalSaveStarted();
  bool AttemptToOfferCvcLocalSave(const CreditCard& card) override;

  // Returns whether AttemptToOfferCvcUploadSave() was called.
  bool CvcUploadSaveStarted();
  void AttemptToOfferCvcUploadSave(const CreditCard& card) override;

  // Returns whether AttemptToOfferCardLocalSave() was called.
  bool CardLocalSaveStarted();
  bool AttemptToOfferCardLocalSave(const CreditCard& card) override;

  void set_show_save_prompt(bool show_save_prompt);

  void set_upload_request_card_number(const std::u16string& credit_card_number);

  void set_upload_request_card(const CreditCard& card);

  payments::PaymentsNetworkInterface::UploadCardRequestDetails*
  upload_request();

  void InitVirtualCardEnroll(
      const CreditCard& credit_card,
      std::optional<payments::PaymentsNetworkInterface::
                        GetDetailsForEnrollmentResponseDetails>
          get_details_for_enrollment_response_details);

  void OnDidUploadCard(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::UploadCardResponseDetails&
          upload_card_response_details) override;

 private:
  bool credit_card_upload_enabled_ = false;
  bool credit_card_was_uploaded_ = false;
  bool cvc_local_save_started_ = false;
  bool cvc_upload_save_started_ = false;
  bool card_local_save_started_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_
