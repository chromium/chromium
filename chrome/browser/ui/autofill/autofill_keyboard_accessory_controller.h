// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"

namespace autofill {

class AutofillKeyboardAccessoryController
    : public AutofillSuggestionController {
 public:
  // TODO(crbug.com/333316034): Rename to `GetWeakPtr` once
  // `AutofillKeyboardAccessoryAdapter` does not exist anymore and there are no
  // nameclashes with `AutofillPopupView`'s methods.
  virtual base::WeakPtr<AutofillKeyboardAccessoryController>
  GetWeakPtrToController() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_H_
