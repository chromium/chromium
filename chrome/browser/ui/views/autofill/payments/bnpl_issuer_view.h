// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_ISSUER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_ISSUER_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill::payments {

class SelectBnplIssuerDialog;

// View containing the list of available BNPL Issuers from which the user may
// select.
class BnplIssuerView : public views::BoxLayoutView {
  METADATA_HEADER(BnplIssuerView, views::BoxLayoutView)

 public:
  explicit BnplIssuerView(
      base::WeakPtr<SelectBnplIssuerDialogController> controller,
      SelectBnplIssuerDialog* issuer_dialog);
  BnplIssuerView(const BnplIssuerView&) = delete;
  BnplIssuerView& operator=(const BnplIssuerView&) = delete;
  ~BnplIssuerView() override;

  void AddedToWidget() override;

 private:
  void IssuerSelected(BnplIssuer issuer, const ui::Event& event);

  const raw_ptr<SelectBnplIssuerDialog> issuer_dialog_;
  base::WeakPtr<SelectBnplIssuerDialogController> controller_;
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_ISSUER_VIEW_H_
