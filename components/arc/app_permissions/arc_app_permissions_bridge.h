// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_APP_PERMISSIONS_ARC_APP_PERMISSIONS_BRIDGE_H_
#define COMPONENTS_ARC_APP_PERMISSIONS_ARC_APP_PERMISSIONS_BRIDGE_H_

#include "base/macros.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcAppPermissionsBridge : public KeyedService {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAppPermissionsBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcAppPermissionsBridge(content::BrowserContext* context,
                          ArcBridgeService* bridge_service);
  ~ArcAppPermissionsBridge() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppPermissionsBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_APP_PERMISSIONS_ARC_APP_PERMISSIONS_BRIDGE_H_
