// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_PILL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_PILL_VIEW_H_

#include "device/fido/discoverable_credential_metadata.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

// A rounded rectangle visualizing user information for a passkey.
class PasskeyPillView : public views::View {
 public:
  METADATA_HEADER(PasskeyPillView);

  explicit PasskeyPillView(const device::PublicKeyCredentialUserEntity& user);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_PILL_VIEW_H_
