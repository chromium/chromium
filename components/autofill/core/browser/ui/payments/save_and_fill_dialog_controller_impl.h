// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_view.h"

namespace autofill {

class SaveAndFillDialogView;

// Implementation of the SaveAndFillDialogController.
class SaveAndFillDialogControllerImpl : public SaveAndFillDialogController {
 public:
  SaveAndFillDialogControllerImpl();
  SaveAndFillDialogControllerImpl(const SaveAndFillDialogControllerImpl&) =
      delete;
  SaveAndFillDialogControllerImpl& operator=(
      const SaveAndFillDialogControllerImpl&) = delete;
  ~SaveAndFillDialogControllerImpl() override;

  void ShowDialog(base::OnceCallback<base::WeakPtr<SaveAndFillDialogView>()>
                      create_and_show_view_callback);
  base::WeakPtr<SaveAndFillDialogController> GetWeakPtr() override;

 private:
  base::WeakPtr<SaveAndFillDialogView> dialog_view_;

  base::WeakPtrFactory<SaveAndFillDialogControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_IMPL_H_
