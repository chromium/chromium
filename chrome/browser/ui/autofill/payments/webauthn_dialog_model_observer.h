// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace autofill {

// The observer for WebauthnDialogModel.
class WebauthnDialogModelObserver : public base::CheckedObserver {
 public:
  virtual void OnDialogStateChanged() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_MODEL_OBSERVER_H_
