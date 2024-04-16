// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"

namespace autofill {

class AutofillPopupController : public AutofillSuggestionController {
 public:
  virtual base::WeakPtr<AutofillPopupController> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
