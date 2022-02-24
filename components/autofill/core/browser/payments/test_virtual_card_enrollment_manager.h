// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"

namespace autofill {

class TestPersonalDataManager;

class TestVirtualCardEnrollmentManager : public VirtualCardEnrollmentManager {
 public:
  TestVirtualCardEnrollmentManager(
      raw_ptr<TestPersonalDataManager> personal_data_manager,
      raw_ptr<payments::TestPaymentsClient> payments_client,
      raw_ptr<TestAutofillClient> autofill_client);
  TestVirtualCardEnrollmentManager(const TestVirtualCardEnrollmentManager&) =
      delete;
  TestVirtualCardEnrollmentManager& operator=(
      const TestVirtualCardEnrollmentManager&) = delete;
  ~TestVirtualCardEnrollmentManager() override;

  bool GetAvatarAnimationComplete() const { return avatar_animation_complete_; }

  bool GetEnrollResponseDetailsReceived() const {
    return enroll_response_details_received_;
  }

  AutofillClient::PaymentsRpcResult GetPaymentsRpcResult() { return result_; }

  void SetPaymentsRpcResult(AutofillClient::PaymentsRpcResult result) {
    result_ = result;
  }

  bool GetResetCalled() { return reset_called_; }

  void SetResetCalled(bool reset_called) { reset_called_ = reset_called; }

  bool GetBubbleShown() { return bubble_shown_; }

  raw_ptr<VirtualCardEnrollmentProcessState>
  GetVirtualCardEnrollmentProcessState() {
    return &state_;
  }

  void SetAutofillClient(raw_ptr<AutofillClient> autofill_client) {
    autofill_client_ = autofill_client;
  }

  void SetVirtualCardEnrollmentFieldsLoadedCallback(
      VirtualCardEnrollmentFieldsLoadedCallback
          virtual_card_enrollment_fields_loaded_callback) {
    virtual_card_enrollment_fields_loaded_callback_ =
        std::move(virtual_card_enrollment_fields_loaded_callback);
  }

  bool AutofillClientIsPresent() { return autofill_client_ != nullptr; }

  // VirtualCardEnrollmentManager:
  void LoadRiskDataAndContinueFlow(
      raw_ptr<PrefService> user_prefs,
      base::OnceCallback<void(const std::string&)> callback) override;
  void OnDidGetUpdateVirtualCardEnrollmentResponse(
      VirtualCardEnrollmentRequestType type,
      AutofillClient::PaymentsRpcResult result) override;
  void Reset() override;
  void ShowVirtualCardEnrollBubble() override;

 private:
  AutofillClient::PaymentsRpcResult result_;

  bool reset_called_ = false;

  bool bubble_shown_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
