// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_and_fill_dialog_views.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"

namespace autofill {

SaveAndFillDialogViews::SaveAndFillDialogViews(
    base::WeakPtr<SaveAndFillDialogController> controller)
    : controller_(controller) {
  SetModalType(ui::mojom::ModalType::kChild);
}

SaveAndFillDialogViews::~SaveAndFillDialogViews() = default;

base::WeakPtr<SaveAndFillDialogView> SaveAndFillDialogViews::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<SaveAndFillDialogView> CreateAndShowSaveAndFillDialog(
    base::WeakPtr<SaveAndFillDialogController> controller,
    content::WebContents* web_contents) {
  SaveAndFillDialogViews* dialog_view = new SaveAndFillDialogViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  return dialog_view->GetWeakPtr();
}

}  // namespace autofill
