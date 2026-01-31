// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Throbber;
}

namespace autofill {

class BnplTosController;

// The dialog delegate view implementation for the Buy-Now-Pay-Later Terms of
// Service view. This is owned by the view hierarchy.
class BnplTosDialog : public views::DialogDelegateView {
  METADATA_HEADER(BnplTosDialog, views::DialogDelegateView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kThrobberId);
  explicit BnplTosDialog(
      base::WeakPtr<BnplTosController> controller,
      base::RepeatingCallback<void(const GURL&)> link_opener);
  BnplTosDialog(const BnplTosDialog&) = delete;
  BnplTosDialog& operator=(const BnplTosDialog&) = delete;
  ~BnplTosDialog() override;

  base::WeakPtr<BnplTosDialog> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // DialogDelegate:
  void AddedToWidget() override;
  void OnWidgetInitialized() override;

  // Callback for when the tab is detached (closed or moved). Logs to
  // BnplTosDialogResult if the dialog's parent window is closed
  void OnTabDetached(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

 private:
  TitleWithIconAfterLabelView::Icon GetTitleIcon() const;
  std::u16string GetTitleIconAccessibilityString() const;
  bool OnAccepted();
  bool OnCancelled();

  base::WeakPtr<BnplTosController> controller_;
  base::RepeatingCallback<void(const GURL&)> link_opener_;

  raw_ptr<views::View> container_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> content_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> throbber_view_ = nullptr;
  raw_ptr<views::Throbber> throbber_ = nullptr;

  base::WeakPtrFactory<BnplTosDialog> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_DIALOG_H_
