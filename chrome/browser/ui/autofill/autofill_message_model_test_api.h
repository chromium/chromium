// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_MODEL_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_MODEL_TEST_API_H_

#include "chrome/browser/ui/autofill/autofill_message_model.h"

namespace autofill {

class AutofillMessageModelTestApi {
 public:
  explicit AutofillMessageModelTestApi(AutofillMessageModel& model)
      : model_(model) {}

  ~AutofillMessageModelTestApi() = default;

  messages::MessageWrapper& GetMessage() { return *model_->message_; }

 private:
  const raw_ref<AutofillMessageModel> model_;
};

inline AutofillMessageModelTestApi test_api(AutofillMessageModel& model) {
  return AutofillMessageModelTestApi(model);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_MODEL_TEST_API_H_
