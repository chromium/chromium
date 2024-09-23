// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_CONTROLLER_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_CONTROLLER_TEST_API_H_

#include "chrome/browser/ui/autofill/payments/autofill_message_controller.h"

namespace autofill {

class AutofillMessageControllerTestApi {
 public:
  explicit AutofillMessageControllerTestApi(
      AutofillMessageController* controller)
      : controller_(*controller) {}

  ~AutofillMessageControllerTestApi() = default;

  std::set<raw_ptr<AutofillMessageModel>> GetMessageModels() {
    std::set<raw_ptr<AutofillMessageModel>> message_models;

    for (auto it = controller_->message_models_.begin();
         it != controller_->message_models_.end(); ++it) {
      message_models.insert(it->get());
    }

    return message_models;
  }

  void Dismiss() { controller_->Dismiss(); }

 private:
  const raw_ref<AutofillMessageController> controller_;
};

inline AutofillMessageControllerTestApi test_api(
    AutofillMessageController& controller) {
  return AutofillMessageControllerTestApi(&controller);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_CONTROLLER_TEST_API_H_
