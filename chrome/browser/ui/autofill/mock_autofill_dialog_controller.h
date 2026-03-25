// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_CONTROLLER_H_

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillDialogController : public AutofillDialogController {
 public:
  MockAutofillDialogController();
  ~MockAutofillDialogController() override;

  MOCK_METHOD(void,
              Show,
              (const std::u16string&,
               const std::u16string&,
               const std::u16string&,
               base::OnceClosure),
              (override));
  MOCK_METHOD(void, OnPositiveButtonClicked, (), (override));
  MOCK_METHOD(void, OnDismissed, (), (override));
  MOCK_METHOD(std::u16string, GetTitleText, (), (const, override));
  MOCK_METHOD(std::u16string, GetDescriptionText, (), (const, override));
  MOCK_METHOD(std::u16string, GetButtonText, (), (const, override));
  MOCK_METHOD(content::WebContents&, GetWebContents, (), (const, override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_CONTROLLER_H_
