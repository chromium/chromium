// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_select_account_sheet_view.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/browser/ui/webauthn/account_hover_list_model.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

AuthenticatorSelectAccountSheetView::AuthenticatorSelectAccountSheetView(
    std::unique_ptr<AuthenticatorSelectAccountSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorSelectAccountSheetView::~AuthenticatorSelectAccountSheetView() =
    default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorSelectAccountSheetView::BuildStepSpecificContent() {
  return std::make_pair(
      std::make_unique<HoverListView>(std::make_unique<AccountHoverListModel>(
          model()->dialog_model(), this)),
      AutoFocus::kYes);
}

void AuthenticatorSelectAccountSheetView::CredentialSelected(size_t index) {
  auto* sheet_model =
      static_cast<AuthenticatorSelectAccountSheetModel*>(model());
  sheet_model->SetCurrentSelection(index);
  sheet_model->OnAccept();
}

BEGIN_METADATA(AuthenticatorSelectAccountSheetView)
END_METADATA
