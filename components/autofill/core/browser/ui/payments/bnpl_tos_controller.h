// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

// Interface that exposes controller functionality to BnplTosView.
class BnplTosController {
 public:
  // Callback received when the BNPL ToS view is to be closed.
  virtual void OnViewClosing() = 0;

  virtual base::WeakPtr<BnplTosController> GetWeakPtr() = 0;

 protected:
  virtual ~BnplTosController() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_
