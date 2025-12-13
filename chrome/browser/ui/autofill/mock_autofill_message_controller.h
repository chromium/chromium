// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_MESSAGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_MESSAGE_CONTROLLER_H_

#include <memory>

#include "chrome/browser/ui/autofill/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillMessageController : public AutofillMessageController {
 public:
  MockAutofillMessageController();
  ~MockAutofillMessageController() override;

  MOCK_METHOD(void, Show, (std::unique_ptr<AutofillMessageModel>), (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_MESSAGE_CONTROLLER_H_
