// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

// Interface that exposes controller functionality to
// SaveAndFillDialogView.
class SaveAndFillDialogController {
 public:
  virtual ~SaveAndFillDialogController() = default;

  virtual base::WeakPtr<SaveAndFillDialogController> GetWeakPtr() = 0;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_H_
