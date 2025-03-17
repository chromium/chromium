// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_SAFETY_ARC_SAFETY_BRIDGE_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_SAFETY_ARC_SAFETY_BRIDGE_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/experiences/arc/mojom/on_device_safety.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcSafetyBridge : public KeyedService, public mojom::OnDeviceSafetyHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSafetyBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcSafetyBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcSafetyBridge(content::BrowserContext* context,
                  ArcBridgeService* bridge_service);

  ArcSafetyBridge(const ArcSafetyBridge&) = delete;
  ArcSafetyBridge& operator=(const ArcSafetyBridge&) = delete;

  ~ArcSafetyBridge() override;

  // OnDeviceSafety Mojo host interface
  void IsCrosSafetyServiceEnabled(
      IsCrosSafetyServiceEnabledCallback callback) override;

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcSafetyBridge> weak_factory_{this};
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_SAFETY_ARC_SAFETY_BRIDGE_H_
