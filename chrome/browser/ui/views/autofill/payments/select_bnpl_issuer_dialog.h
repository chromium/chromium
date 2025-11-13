// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Throbber;
class View;
}  // namespace views

namespace autofill::payments {

class BnplDialogFootnote;
class BnplIssuerView;
class SelectBnplIssuerDialogController;

// The desktop dialog view implementation for the BNPL Issuer dialog.
class SelectBnplIssuerDialog : public views::DialogDelegateView {
  METADATA_HEADER(SelectBnplIssuerDialog, views::DialogDelegateView)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kThrobberId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBnplIssuerView);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFootnoteViewId);
  explicit SelectBnplIssuerDialog(
      base::WeakPtr<SelectBnplIssuerDialogController> controller,
      content::WebContents* web_contents,
      bool has_seen_ai_terms);
  SelectBnplIssuerDialog(const SelectBnplIssuerDialog&) = delete;
  SelectBnplIssuerDialog& operator=(const SelectBnplIssuerDialog&) = delete;
  ~SelectBnplIssuerDialog() override;

  base::WeakPtr<SelectBnplIssuerDialog> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void DisplayThrobber();
  void DismissThrobberAndShowIssuerView();
  bool OnCancelled();

  // View:
  void AddedToWidget() override;

 private:
  raw_ptr<views::View> container_view_ = nullptr;
  raw_ptr<BnplIssuerView> bnpl_issuer_view_ = nullptr;
  raw_ptr<views::View> throbber_container_view_ = nullptr;
  raw_ptr<views::Throbber> throbber_ = nullptr;
  raw_ptr<BnplDialogFootnote> bnpl_footnote_view_ = nullptr;

  base::WeakPtr<SelectBnplIssuerDialogController> controller_;

  // The web contents used to open the settings footer link.
  base::WeakPtr<content::WebContents> web_contents_;

  // Creates the throbber view.
  void CreateThrobberView();

  // Creates the BNPL issuer view.
  void CreateBnplIssuerView();

  // Called when the user clicks the "payment settings" link in the dialog.
  // Opens the Chrome Payment settings page, to allow the user to manage their
  // saved payment methods.
  void OnSettingsLinkClicked();

  base::WeakPtrFactory<SelectBnplIssuerDialog> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_H_
