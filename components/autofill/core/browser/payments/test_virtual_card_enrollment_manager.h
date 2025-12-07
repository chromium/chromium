// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_

#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

namespace autofill {

class PaymentsDataManager;

class TestVirtualCardEnrollmentManager : public VirtualCardEnrollmentManager {
 public:
  TestVirtualCardEnrollmentManager(
      PaymentsDataManager* payments_data_manager,
      PaymentsNetworkInterfaceVariation payments_network_interface,
      TestAutofillClient* autofill_client);
  TestVirtualCardEnrollmentManager(const TestVirtualCardEnrollmentManager&) =
      delete;
  TestVirtualCardEnrollmentManager& operator=(
      const TestVirtualCardEnrollmentManager&) = delete;
  ~TestVirtualCardEnrollmentManager() override;

  bool GetEnrollResponseDetailsReceived() const {
    return enroll_response_details_received_;
  }

  void SetEnrollResponseDetailsReceived(bool enroll_response_details_received) {
    enroll_response_details_received_ = enroll_response_details_received;
  }

  payments::PaymentsAutofillClient::PaymentsRpcResult GetPaymentsRpcResult() {
    return result_;
  }

  void SetPaymentsRpcResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult result) {
    result_ = result;
  }

  bool GetResetCalled() { return reset_called_; }

  void SetResetCalled(bool reset_called) { reset_called_ = reset_called; }

  bool GetBubbleShown() { return bubble_shown_; }

  void SetBubbleShown(bool bubble_shown) { bubble_shown_ = bubble_shown; }

  VirtualCardEnrollmentProcessState* GetVirtualCardEnrollmentProcessState() {
    return &state_;
  }

  void ResetVirtualCardEnrollmentProcessState() {
    state_ = VirtualCardEnrollmentProcessState();
  }

  void SetAutofillClient(AutofillClient* autofill_client) {
    autofill_client_ = autofill_client;
  }

  void set_ignore_strike_database(bool ignore_strike_database) {
    ignore_strike_database_ = ignore_strike_database;
  }

  bool AutofillClientIsPresent() { return autofill_client_ != nullptr; }

  // VirtualCardEnrollmentManager:
  bool ShouldBlockVirtualCardEnrollment(
      const std::string& instrument_id,
      VirtualCardEnrollmentSource virtual_card_enrollment_source)
      const override;
  void LoadRiskDataAndContinueFlow(
      PrefService* user_prefs,
      base::OnceCallback<void(const std::string&)> callback) override;
  void OnDidGetUpdateVirtualCardEnrollmentResponse(
      VirtualCardEnrollmentRequestType type,
      payments::PaymentsAutofillClient::PaymentsRpcResult result) override;
  void Reset() override;
  void ShowVirtualCardEnrollBubble(
      VirtualCardEnrollmentFields* virtual_card_enrollment_fields) override;

  void OnVirtualCardEnrollmentBubbleCancelled();

 private:
  payments::PaymentsAutofillClient::PaymentsRpcResult result_;

  bool reset_called_ = false;

  bool bubble_shown_ = false;

  // Whether StrikeDatabase rules should be ignored when calling
  // InitVirtualCardEnroll. Used to set up tests more easily.
  bool ignore_strike_database_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
