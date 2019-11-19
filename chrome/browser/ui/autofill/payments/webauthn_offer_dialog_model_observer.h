// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace autofill {

// The observer for WebauthnOfferDialogModel.
class WebauthnOfferDialogModelObserver : public base::CheckedObserver {
 public:
  virtual void OnDialogStateChanged() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_MODEL_OBSERVER_H_
