// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_

namespace autofill::payments {

// The cross-platform interface that handles the UI for the BNPL autofill flows.
class BnplUiDelegate {
 public:
  virtual ~BnplUiDelegate() = default;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_
