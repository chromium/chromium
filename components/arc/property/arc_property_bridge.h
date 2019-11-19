// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_PROPERTY_ARC_PROPERTY_BRIDGE_H_
#define COMPONENTS_ARC_PROPERTY_ARC_PROPERTY_BRIDGE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "components/arc/mojom/property.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// ARC Property Client gets system properties from ARC instances.
class ArcPropertyBridge : public KeyedService,
                          public ConnectionObserver<mojom::PropertyInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPropertyBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcPropertyBridge(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcPropertyBridge() override;

  // ConnectionObserver<mojom::PropertyInstance> overrides:
  void OnConnectionReady() override;

  void GetGcaMigrationProperty(
      mojom::PropertyInstance::GetGcaMigrationPropertyCallback callback);

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // Store pending requests when connection is not ready.
  std::vector<mojom::PropertyInstance::GetGcaMigrationPropertyCallback>
      pending_requests_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ArcPropertyBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_PROPERTY_ARC_PROPERTY_BRIDGE_H_
