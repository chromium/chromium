// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_COMMON_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_COMMON_VIEWS_H_

#include <memory>
#include <string>

namespace views {
class View;
}  // namespace views

// Creates a view for the passkey to be created.
// +---------------------+
// |           username  |
// | <icon>              |
// |           Passkey   |
// +---------------------+
std::unique_ptr<views::View> CreatePasskeyWithUsernameLabel(
    std::u16string username);

// Creates a simple view with a password manager icon and a label.
// +---------------------+
// | <icon>      label   |
// +---------------------+
std::unique_ptr<views::View> CreateGpmIconWithLabel();

constexpr int kWebAuthnGpmDialogSpacingBetweenTitleAndDescription = 4;

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_COMMON_VIEWS_H_
