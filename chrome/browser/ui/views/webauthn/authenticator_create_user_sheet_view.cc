// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_create_user_sheet_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/webauthn/authenticator_common_views.h"
#include "ui/base/metadata/metadata_impl_macros.h"

AuthenticatorCreateUserSheetView::AuthenticatorCreateUserSheetView(
    std::unique_ptr<AuthenticatorSheetModelBase> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorCreateUserSheetView::~AuthenticatorCreateUserSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorCreateUserSheetView::AutoFocus>
AuthenticatorCreateUserSheetView::BuildStepSpecificContent() {
  auto* sheet_model = static_cast<AuthenticatorSheetModelBase*>(model());
  std::u16string username = base::UTF8ToUTF16(
      sheet_model->dialog_model()->user_entity.name.value_or(""));
  return std::make_pair(CreatePasskeyWithUsernameLabel(username),
                        AutoFocus::kNo);
}

BEGIN_METADATA(AuthenticatorCreateUserSheetView)
END_METADATA
