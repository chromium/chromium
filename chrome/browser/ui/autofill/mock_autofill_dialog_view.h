// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_H_

#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillDialogView : public AutofillDialogView {
 public:
  MockAutofillDialogView();
  ~MockAutofillDialogView() override;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, Dismiss, (), (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_H_
