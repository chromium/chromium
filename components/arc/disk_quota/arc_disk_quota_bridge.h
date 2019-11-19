// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_
#define COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_

#include "base/macros.h"
#include "components/arc/mojom/disk_quota.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class proxies quota requests from Android to cryptohome.
class ArcDiskQuotaBridge : public KeyedService, public mojom::DiskQuotaHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcDiskQuotaBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcDiskQuotaBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcDiskQuotaBridge() override;

  // mojom::DiskQuotaHost overrides:
  void IsQuotaSupported(IsQuotaSupportedCallback callback) override;

  void GetCurrentSpaceForUid(uint32_t uid,
                             GetCurrentSpaceForUidCallback callback) override;

  void GetCurrentSpaceForGid(uint32_t gid,
                             GetCurrentSpaceForGidCallback callback) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  DISALLOW_COPY_AND_ASSIGN(ArcDiskQuotaBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_
