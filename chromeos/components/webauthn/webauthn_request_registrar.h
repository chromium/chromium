// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_WEBAUTHN_WEBAUTHN_REQUEST_REGISTRAR_H_
#define CHROMEOS_COMPONENTS_WEBAUTHN_WEBAUTHN_REQUEST_REGISTRAR_H_

#include <stdint.h>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace aura {
class Window;
}

namespace chromeos {
namespace webauthn {

// Provides service to associate webauthn request ids with windows.
class COMPONENT_EXPORT(CHROMEOS_WEBAUTHN) WebAuthnRequestRegistrar {
 public:
  // Returns the singleton instance.
  static WebAuthnRequestRegistrar* Get();

  // Returns a callback to generate request id for |window|. The callback is
  // not thread-safe, and must be invoked from the browser UI thread only.
  using GenerateRequestIdCallback = base::RepeatingCallback<std::string()>;
  virtual GenerateRequestIdCallback GetRegisterCallback(
      aura::Window* window) = 0;

  // Returns the window that was registered with |request_id|, or nullptr if no
  // such window.
  virtual aura::Window* GetWindowForRequestId(std::string request_id) = 0;

 protected:
  WebAuthnRequestRegistrar();
  virtual ~WebAuthnRequestRegistrar();
};

}  // namespace webauthn
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_WEBAUTHN_WEBAUTHN_REQUEST_REGISTRAR_H_
