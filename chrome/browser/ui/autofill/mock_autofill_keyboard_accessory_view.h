// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillKeyboardAccessoryView : public AutofillKeyboardAccessoryView {
 public:
  MockAutofillKeyboardAccessoryView();
  ~MockAutofillKeyboardAccessoryView() override;

  MOCK_METHOD(bool, Initialize, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, AxAnnounce, (const std::u16string&), (override));
  MOCK_METHOD(void,
              ConfirmDeletion,
              (const std::u16string&,
               const std::u16string&,
               base::OnceCallback<void(bool)>),
              (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_
