// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_priority_mechanism_sheet_view.h"

#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"

AuthenticatorPriorityMechanismSheetView::
    AuthenticatorPriorityMechanismSheetView(
        std::unique_ptr<AuthenticatorPriorityMechanismSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}

AuthenticatorPriorityMechanismSheetView::
    ~AuthenticatorPriorityMechanismSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorPriorityMechanismSheetView::BuildStepSpecificContent() {
  auto* sheet_model =
      static_cast<AuthenticatorPriorityMechanismSheetModel*>(model());
  return std::make_pair(
      std::make_unique<HoverListView>(std::make_unique<TransportHoverListModel>(
          sheet_model->dialog_model(),
          std::vector{base::checked_cast<int>(
              *sheet_model->dialog_model()->priority_mechanism_index)})),
      AutoFocus::kNo);
}

BEGIN_METADATA(AuthenticatorPriorityMechanismSheetView)
END_METADATA
