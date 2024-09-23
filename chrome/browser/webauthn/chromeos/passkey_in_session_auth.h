// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_IN_SESSION_AUTH_H_
#define CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_IN_SESSION_AUTH_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace aura {
class Window;
}

namespace chromeos {

// PasskeyInSessionAuthProvider is responsible for showing the ChromeOS user
// verification dialog when creating or asserting a passkey. It can be used from
// Ash and Lacros.
class PasskeyInSessionAuthProvider {
 public:
  static PasskeyInSessionAuthProvider* Get();
  static void SetInstanceForTesting(
      PasskeyInSessionAuthProvider* test_override);

  virtual ~PasskeyInSessionAuthProvider();

  virtual void ShowPasskeyInSessionAuthDialog(
      aura::Window* window,
      const std::string& rp_id,
      base::OnceCallback<void(bool)> result_callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_IN_SESSION_AUTH_H_
