// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_DETAIL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_DETAIL_VIEW_H_

#include "device/fido/discoverable_credential_metadata.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

// A view displaying user information for a passkey.
class PasskeyDetailView : public views::View {
  METADATA_HEADER(PasskeyDetailView, views::View)

 public:
  explicit PasskeyDetailView(const device::PublicKeyCredentialUserEntity& user);

  // views::View:
  void OnThemeChanged() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_DETAIL_VIEW_H_
