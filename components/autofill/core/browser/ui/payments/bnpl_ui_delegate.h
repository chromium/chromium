// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace autofill {

struct AutofillErrorDialogContext;
enum class AutofillProgressDialogType;
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
      base::OnceClosure cancel_callback,
      bool has_seen_ai_terms) = 0;

  // Update the issuer selection dialog based on `issuer_contexts`.
  // `issuer_contexts` indicates the issuers' status based on the received
  // amount and the issuers' eligibility.
  virtual void UpdateBnplIssuerDialogUi(
      std::vector<BnplIssuerContext> issuer_contexts) = 0;

  // Dismisses the BNPL issuer selection UI or hides the progress UI depending
  // on the platform.
  virtual void RemoveSelectBnplIssuerOrProgressUi() = 0;

  // Shows a view that presents the BNPL Terms of Service UI to the user to
  // accept or decline.
  virtual void ShowBnplTosUi(BnplTosModel bnpl_tos_model,
                             base::OnceClosure accept_callback,
                             base::OnceClosure cancel_callback) = 0;

  // Closes the BNPL Terms of Service UI that was displayed in `ShowBnplTos()`
  // on Desktop, and hides the progress UI that was shown after ToS UI
  // accepted on Android.
  virtual void RemoveBnplTosOrProgressUi() = 0;

  // Shows a progress UI while fetching the VCN. `autofill_progress_dialog_type`
  // determines the specific UI of the progress dialog shown. `cancel_callback`
  // is run when the user cancels the progress dialog.
  virtual void ShowProgressUi(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback) = 0;

  // Closes the progress UI. `credit_card_fetched_successfully` is true if the
  // form was successfully filled with the BNPL VCN, and is false otherwise.
  virtual void CloseProgressUi(bool credit_card_fetched_successfully) = 0;

  // Shows an error UI during the BNPL flow. The type of error UI that is shown
  // will match the `type` in `context`. If the `server_returned_title` and
  // `server_returned_description` in `context` are both set, the error dialog
  // that is displayed will have these fields displayed for the title and
  // description, respectively.
  virtual void ShowAutofillErrorUi(AutofillErrorDialogContext context) = 0;
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_UI_DELEGATE_H_
