// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEWS_H_

#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

class SaveAndFillDialogController;

class SaveAndFillDialogViews : public SaveAndFillDialogView,
                               public views::DialogDelegateView {
 public:
  explicit SaveAndFillDialogViews(
      base::WeakPtr<SaveAndFillDialogController> controller);
  SaveAndFillDialogViews(const SaveAndFillDialogViews&) = delete;
  SaveAndFillDialogViews& operator=(const SaveAndFillDialogViews&) = delete;
  ~SaveAndFillDialogViews() override;

  // SaveAndFillDialogView:
  base::WeakPtr<SaveAndFillDialogView> GetWeakPtr() override;

 private:
  base::WeakPtr<SaveAndFillDialogController> controller_;

  base::WeakPtrFactory<SaveAndFillDialogViews> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEWS_H_
