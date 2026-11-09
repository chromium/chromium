// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/webauthn_ui_helpers.h"

#include <cstddef>
#include <string>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "device/fido/public/fido_types.h"
#include "ui/gfx/font_list.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace webauthn_ui_helpers {

std::u16string RpIdToElidedHost(const std::string& relying_party_id,
                                size_t width,
                                gfx::FontList font_list) {
  GURL relying_party_id_url(base::StrCat(
      {url::kHttpsScheme, url::kStandardSchemeSeparator, relying_party_id}));
  CHECK(relying_party_id_url.is_valid(), base::NotFatalUntil::M162);
  return url_formatter::ElideHost(relying_party_id_url, font_list, width);
}

const gfx::VectorIcon* GetProviderIcon(
    const AuthenticatorRequestDialogModel::Mechanism::CredentialInfo&
        credential) {
  switch (credential.source) {
    case device::AuthenticatorType::kICloudKeychain:
      // Only show the Apple Passwords 3-key icon when the credential
      // originated from Apple's built-in Passwords app on macOS 15+.
      // Third-party credential provider extensions and pre-macOS 15 ("iCloud
      // Keychain") have no provider icon (see crbug.com/578112294).
      return credential.is_apple_passwords ? &kApplePasswordsColorCustomIcon
                                           : nullptr;
    case device::AuthenticatorType::kEnclave:
      return &GooglePasswordManagerVectorIcon();
    case device::AuthenticatorType::kWinNative:
      return &kWindowsHelloColorCustomIcon;
    case device::AuthenticatorType::kTouchID:
      return &vector_icons::kProductRefreshIcon;
    default:
      return nullptr;
  }
}

}  // namespace webauthn_ui_helpers
