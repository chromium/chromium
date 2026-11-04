// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_WEBAUTHN_UI_HELPERS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_WEBAUTHN_UI_HELPERS_H_

#include <cstddef>
#include <string>

#include "ui/gfx/font_list.h"

namespace device {
enum class AuthenticatorType;
}  // namespace device

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace webauthn_ui_helpers {

// Takes a valid relying party identifier and elides it so that it's suitable to
// display on UI.
std::u16string RpIdToElidedHost(const std::string& relying_party_id,
                                size_t width,
                                gfx::FontList font_list);

// Returns the icon of the provider that stores a passkey of `type`, e.g. Google
// Password Manager, or null if the provider has no icon.
const gfx::VectorIcon* GetProviderIcon(device::AuthenticatorType type);

}  // namespace webauthn_ui_helpers

#endif  // CHROME_BROWSER_UI_WEBAUTHN_WEBAUTHN_UI_HELPERS_H_
