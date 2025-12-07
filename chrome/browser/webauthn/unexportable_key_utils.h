// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_UNEXPORTABLE_KEY_UTILS_H_
#define CHROME_BROWSER_WEBAUTHN_UNEXPORTABLE_KEY_UTILS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "crypto/user_verifying_key.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <variant>
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
namespace ash {
class WebAuthNDialogController;
class ActiveSessionAuthController;
}

namespace aura {
class Window;
}
#endif

namespace crypto {
class UnexportableKeyProvider;
}  // namespace crypto

std::unique_ptr<crypto::UnexportableKeyProvider>
GetWebAuthnUnexportableKeyProvider();

#if BUILDFLAG(IS_CHROMEOS)
struct UserVerifyingKeyProviderConfigChromeos {
  using AuthDialogController =
      std::variant<raw_ptr<ash::WebAuthNDialogController>,
                   raw_ptr<ash::ActiveSessionAuthController>>;

  UserVerifyingKeyProviderConfigChromeos(AuthDialogController dialog_controller,
                                         aura::Window* window,
                                         std::string rp_id);
  UserVerifyingKeyProviderConfigChromeos(
      const UserVerifyingKeyProviderConfigChromeos&);
  UserVerifyingKeyProviderConfigChromeos& operator=(
      const UserVerifyingKeyProviderConfigChromeos&);
  ~UserVerifyingKeyProviderConfigChromeos();

  AuthDialogController dialog_controller;

  // The source window to which to anchor the dialog.
  raw_ptr<aura::Window> window;

  // The Relying Party ID shown in the confirmation prompt.
  std::string rp_id;
};

std::unique_ptr<crypto::UserVerifyingKeyProvider>
GetWebAuthnUserVerifyingKeyProvider(
    UserVerifyingKeyProviderConfigChromeos config);
#else
std::unique_ptr<crypto::UserVerifyingKeyProvider>
GetWebAuthnUserVerifyingKeyProvider(
    crypto::UserVerifyingKeyProvider::Config config);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// ChromeOS doesn't use the UserVerifyingKeyProvider provided by //crypto, so
// the test override is handled separately as well.
void OverrideWebAuthnChromeosUserVerifyingKeyProviderForTesting(
    std::unique_ptr<crypto::UserVerifyingKeyProvider> (*func)());
#endif

void SetWebAuthnUnexportableKeyProviderForTesting(
    std::unique_ptr<crypto::UnexportableKeyProvider> (*func)());

#endif  // CHROME_BROWSER_WEBAUTHN_UNEXPORTABLE_KEY_UTILS_H_
