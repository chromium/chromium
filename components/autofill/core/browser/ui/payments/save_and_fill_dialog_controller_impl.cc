// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"

#include "base/memory/weak_ptr.h"

namespace autofill {

SaveAndFillDialogControllerImpl::SaveAndFillDialogControllerImpl() = default;
SaveAndFillDialogControllerImpl::~SaveAndFillDialogControllerImpl() = default;

void SaveAndFillDialogControllerImpl::ShowDialog(
    base::OnceCallback<base::WeakPtr<SaveAndFillDialogView>()>
        create_and_show_view_callback) {
  dialog_view_ = std::move(create_and_show_view_callback).Run();
  CHECK(dialog_view_);
}

base::WeakPtr<SaveAndFillDialogController>
SaveAndFillDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
