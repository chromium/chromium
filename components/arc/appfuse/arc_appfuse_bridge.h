// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_APPFUSE_ARC_APPFUSE_BRIDGE_H_
#define COMPONENTS_ARC_APPFUSE_ARC_APPFUSE_BRIDGE_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/arc/mojom/appfuse.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles Appfuse mount/unmount requests from the ARC container.
class ArcAppfuseBridge : public KeyedService, public mojom::AppfuseHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAppfuseBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcAppfuseBridge(content::BrowserContext* context,
                   ArcBridgeService* bridge_service);
  ~ArcAppfuseBridge() override;

  // mojom::AppfuseHost overrides:
  void Mount(uint32_t uid, int32_t mount_id, MountCallback callback) override;
  void Unmount(uint32_t uid,
               int32_t mount_id,
               UnmountCallback callback) override;
  void OpenFile(uint32_t uid,
                int32_t mount_id,
                int32_t file_id,
                int32_t flags,
                OpenFileCallback callback) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  DISALLOW_COPY_AND_ASSIGN(ArcAppfuseBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_APPFUSE_ARC_APPFUSE_BRIDGE_H_
