// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_TEST_API_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"

namespace autofill {

// Test-only methods for VirtualCardEnrollmentUiModel.
class VirtualCardEnrollUiModelTestApi {
 public:
  explicit VirtualCardEnrollUiModelTestApi(VirtualCardEnrollUiModel* model)
      : model_(*model) {}

  // Allow modifying properties of the model in tests.
  std::u16string& window_title() { return model_->window_title_; }
  std::u16string& explanatory_message() { return model_->explanatory_message_; }
  std::u16string& accept_action_text() { return model_->accept_action_text_; }
  std::u16string& cancel_action_text() { return model_->cancel_action_text_; }
  std::u16string& learn_more_link_text() {
    return model_->learn_more_link_text_;
  }
  VirtualCardEnrollmentFields& enrollment_fields() {
    return model_->enrollment_fields_;
  }

 private:
  // The target VirtualCardEnrollUiModel instance of this test api.
  const raw_ref<VirtualCardEnrollUiModel> model_;
};

inline VirtualCardEnrollUiModelTestApi test_api(
    VirtualCardEnrollUiModel& ui_model) {
  return VirtualCardEnrollUiModelTestApi(&ui_model);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_TEST_API_H_
