// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_KEYMASTER_ARC_KEYMASTER_BRIDGE_H_
#define COMPONENTS_ARC_KEYMASTER_ARC_KEYMASTER_BRIDGE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/mojom/keymaster.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class is responsible for providing a KeymasterServer proxy by
// bootstrapping a mojo connection with the arc-keymasterd daemon. The mojo
// connection is bootstrapped lazily during the first call to GetServer. Chrome
// has no further involvement once the KeymasterServer proxy has been forwarded
// to the KeymasterInstance in ARC.
class ArcKeymasterBridge : public KeyedService, public mojom::KeymasterHost {
 public:
  // Returns singleton instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcKeymasterBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcKeymasterBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcKeymasterBridge() override;

  // KeymasterHost mojo interface.
  using mojom::KeymasterHost::GetServerCallback;
  void GetServer(GetServerCallback callback) override;

 private:
  void BootstrapMojoConnection(GetServerCallback callback);
  void OnBootstrapMojoConnection(GetServerCallback callback, bool result);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // Points to a proxy bound to the implementation in arc-keymasterd.
  mojom::KeymasterServerPtr keymaster_server_proxy_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcKeymasterBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcKeymasterBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_KEYMASTER_ARC_KEYMASTER_BRIDGE_H_
