// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_DISCOVERY_H_
#define CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_DISCOVERY_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/chromeos/passkey_authenticator.h"
#include "content/public/browser/global_routing_id.h"
#include "device/fido/fido_discovery_base.h"

namespace content {
class RenderFrameHost;
}

namespace chromeos {

class PasskeyAuthenticator;

class PasskeyDiscovery : public device::FidoDiscoveryBase {
 public:
  explicit PasskeyDiscovery(content::RenderFrameHost* rfh);
  ~PasskeyDiscovery() override;

  void StartUI();

  // device::FidoDiscoveryBase:
  void Start() override;

 private:
  void StartDiscovery();

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  std::vector<std::unique_ptr<PasskeyAuthenticator>> authenticators_;
  base::WeakPtrFactory<PasskeyDiscovery> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_DISCOVERY_H_
