// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_transport_selector_sheet_view.h"

AuthenticatorTransportSelectorSheetView::
    AuthenticatorTransportSelectorSheetView(
        std::unique_ptr<AuthenticatorTransportSelectorSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}

AuthenticatorTransportSelectorSheetView::
    ~AuthenticatorTransportSelectorSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorTransportSelectorSheetView::BuildStepSpecificContent() {
  return std::make_pair(
      std::make_unique<HoverListView>(std::make_unique<TransportHoverListModel>(
          model()->dialog_model()->mechanisms())),
      AutoFocus::kYes);
}
