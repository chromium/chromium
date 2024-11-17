// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_select_account_sheet_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/browser/ui/views/webauthn/passkey_detail_view.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "ui/base/metadata/metadata_impl_macros.h"

AuthenticatorSelectAccountSheetView::AuthenticatorSelectAccountSheetView(
    std::unique_ptr<AuthenticatorSelectAccountSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorSelectAccountSheetView::~AuthenticatorSelectAccountSheetView() =
    default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorSelectAccountSheetView::BuildStepSpecificContent() {
  switch (model()->selection_type()) {
    case AuthenticatorSelectAccountSheetModel::kMultipleAccounts:
      return std::make_pair(std::make_unique<HoverListView>(
                                std::make_unique<AccountHoverListModel>(
                                    model()->dialog_model(), this)),
                            AutoFocus::kYes);
    case AuthenticatorSelectAccountSheetModel::kSingleAccount:
      return std::make_pair(
          std::make_unique<PasskeyDetailView>(model()->SingleCredential().user),
          AutoFocus::kNo);
  }
}

void AuthenticatorSelectAccountSheetView::CredentialSelected(size_t index) {
  auto* sheet_model =
      static_cast<AuthenticatorSelectAccountSheetModel*>(model());
  sheet_model->SetCurrentSelection(index);
  sheet_model->OnAccept();
}

BEGIN_METADATA(AuthenticatorSelectAccountSheetView)
END_METADATA
