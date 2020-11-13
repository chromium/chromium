// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_VIEW_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

class VirtualCardSelectionDialogController;

// The Views implementation of VirtualCardSelectionDialogView.
class VirtualCardSelectionDialogViewImpl
    : public VirtualCardSelectionDialogView,
      public views::DialogDelegateView {
 public:
  VirtualCardSelectionDialogViewImpl(
      VirtualCardSelectionDialogController* controller);
  ~VirtualCardSelectionDialogViewImpl() override;

  // VirtualCardSelectionDialogView:
  void Hide() override;

  // views::DialogDelegateView:
  void AddedToWidget() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  View* GetContentsView() override;
  base::string16 GetWindowTitle() const override;

 private:
  VirtualCardSelectionDialogController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VirtualCardSelectionDialogViewImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_VIEW_IMPL_H_
