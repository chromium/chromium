// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace autofill {

class BnplIssuer;
struct BnplTosModel;

namespace payments {

struct BnplIssuerContext;

// The cross-platform interface that handles the UI for the BNPL autofill flows.
class BnplUiDelegate {
 public:
  virtual ~BnplUiDelegate() = default;

  // Shows the issuer selection UI for BNPL when the BNPL suggestion is
  // selected to let users choose a BNPL issuer.
  virtual void ShowSelectBnplIssuerUi(
      std::vector<BnplIssuerContext> bnpl_issuer_context,
      std::string app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) = 0;

  // Dismiss the issuer selection UI for BNPL.
  virtual void DismissSelectBnplIssuerUi() = 0;

  // Shows a view that presents the BNPL Terms of Service UI to the user to
  // accept or decline.
  virtual void ShowBnplTosUi(BnplTosModel bnpl_tos_model,
                             base::OnceClosure accept_callback,
                             base::OnceClosure cancel_callback) = 0;

  // Closes the BNPL Terms of Service UI that was displayed in
  // `ShowBnplTos()`.
  virtual void CloseBnplTosUi() = 0;
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_
