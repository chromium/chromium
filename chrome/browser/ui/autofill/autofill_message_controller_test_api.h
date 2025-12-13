// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_TEST_API_H_

#include "chrome/browser/ui/autofill/autofill_message_controller_impl.h"

namespace autofill {

class AutofillMessageControllerTestApi {
 public:
  explicit AutofillMessageControllerTestApi(
      AutofillMessageControllerImpl& controller)
      : controller_(controller) {}

  ~AutofillMessageControllerTestApi() = default;

  std::set<raw_ptr<AutofillMessageModel>> GetMessageModels() {
    std::set<raw_ptr<AutofillMessageModel>> message_models;

    for (auto it = controller_->message_models_.begin();
         it != controller_->message_models_.end(); ++it) {
      message_models.insert(it->get());
    }

    return message_models;
  }

  void OnActionClicked(AutofillMessageModel* message_model_ptr) {
    controller_->OnActionClicked(message_model_ptr);
  }

  void OnDismissed(AutofillMessageModel* message_model_ptr,
                   messages::DismissReason reason) {
    controller_->OnDismissed(message_model_ptr, reason);
  }

  void Dismiss() { controller_->Dismiss(); }

 private:
  const raw_ref<AutofillMessageControllerImpl> controller_;
};

inline AutofillMessageControllerTestApi test_api(
    AutofillMessageControllerImpl& controller) {
  return AutofillMessageControllerTestApi(controller);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_TEST_API_H_
