// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_MULTI_SOURCE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_MULTI_SOURCE_PICKER_VIEW_H_

#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// Content view displaying one or two lists to select between multiple Web
// Authentication accounts and mechanisms.
class AuthenticatorMultiSourcePickerView : public views::View {
  METADATA_HEADER(AuthenticatorMultiSourcePickerView, views::View)

 public:
  explicit AuthenticatorMultiSourcePickerView(
      AuthenticatorMultiSourcePickerSheetModel* model);

  AuthenticatorMultiSourcePickerView(
      const AuthenticatorMultiSourcePickerView&) = delete;
  AuthenticatorMultiSourcePickerView& operator=(
      const AuthenticatorMultiSourcePickerView&) = delete;

  ~AuthenticatorMultiSourcePickerView() override;

 private:
  // views::View:
  void RequestFocus() override;

  raw_ptr<views::View> primary_passkeys_control_ = nullptr;
  raw_ptr<views::View> secondary_passkeys_control_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_MULTI_SOURCE_PICKER_VIEW_H_
