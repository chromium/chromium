// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_dialog.h"

#include "ui/base/metadata/metadata_impl_macros.h"

namespace autofill {

BEGIN_METADATA(BnplTosDialog)
END_METADATA

BnplTosDialog::BnplTosDialog(base::WeakPtr<BnplTosController> controller)
    : controller_(controller) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following two lines once this is the
  // default state for widgets and the delegates.
  SetOwnedByWidget(false);
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  SetModalType(ui::mojom::ModalType::kChild);
}

BnplTosDialog::~BnplTosDialog() = default;

BnplTosController* BnplTosDialog::controller() const {
  return controller_.get();
}

}  // namespace autofill
