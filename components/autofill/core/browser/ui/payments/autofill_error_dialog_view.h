// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

// The cross-platform view interface which helps show an error dialog for
// autofill flows.
//
// Note: This is only used for virtual card related errors.
class AutofillErrorDialogView {
 public:
  virtual ~AutofillErrorDialogView() = default;

  virtual void Dismiss() = 0;

  virtual base::WeakPtr<AutofillErrorDialogView> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_
