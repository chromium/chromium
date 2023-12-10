// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"

namespace autofill {

namespace payments {
class TestPaymentsNetworkInterface;
}  // namespace payments

class AutofillClient;
class AutofillDriver;
class PersonalDataManager;

class TestCreditCardSaveManager : public CreditCardSaveManager {
 public:
  TestCreditCardSaveManager(
      AutofillDriver* driver,
      AutofillClient* client,
      payments::TestPaymentsNetworkInterface* payments_network_interface,
      PersonalDataManager* personal_data_manager);

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

  payments::PaymentsNetworkInterface::UploadRequestDetails* upload_request();

 private:
  void OnDidUploadCard(
      AutofillClient::PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::UploadCardResponseDetails&
          upload_card_response_details) override;

  bool credit_card_upload_enabled_ = false;
  bool credit_card_was_uploaded_ = false;
  bool cvc_local_save_started_ = false;
  bool cvc_upload_save_started_ = false;
  bool card_local_save_started_ = false;

  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveManagerTest,
                           OnDidUploadCard_DoNotAddServerCvcIfCvcIsEmpty);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardSaveManagerTest,
      OnDidUploadCard_DoNotAddServerCvcIfInstrumentIdIsEmpty);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveManagerTest,
                           OnDidUploadCard_VirtualCardEnrollment);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardSaveManagerTest,
      OnDidUploadCard_VirtualCardEnrollment_GetDetailsForEnrollmentResponseDetailsReturned);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveManagerTest,
                           UploadCreditCard_NumStrikesLoggedOnUploadNotSuccess);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardSaveManagerWithLocalSaveFallbackTest,
      OnDidUploadCard_FallbackToLocalSaveOnServerUploadFailure);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardSaveManagerWithLocalSaveFallbackTest,
      OnDidUploadCard_SkipLocalSaveIfMissingExpirationDate);
  FRIEND_TEST_ALL_PREFIXES(SaveCvcTest, OnDidUploadCard_SaveServerCvc);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_MANAGER_H_
