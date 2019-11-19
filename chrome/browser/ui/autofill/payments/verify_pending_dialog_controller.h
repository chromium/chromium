// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_CONTROLLER_H_

#include "base/macros.h"
#include "base/strings/string16.h"

namespace autofill {

// An interface that exposes necessary controller functionality to
// VerifyPendingDialogView.
class VerifyPendingDialogController {
 public:
  VerifyPendingDialogController() = default;
  virtual ~VerifyPendingDialogController() = default;

  virtual base::string16 GetDialogTitle() const = 0;

  virtual void OnCancel() = 0;
  virtual void OnDialogClosed() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VerifyPendingDialogController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_CONTROLLER_H_
