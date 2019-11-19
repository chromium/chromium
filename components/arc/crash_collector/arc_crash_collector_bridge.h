// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_
#define COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "components/arc/mojom/crash_collector.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Relays dumps for non-native ARC crashes to the crash reporter in Chrome OS.
class ArcCrashCollectorBridge
    : public KeyedService,
      public mojom::CrashCollectorHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcCrashCollectorBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcCrashCollectorBridge(content::BrowserContext* context,
                          ArcBridgeService* bridge);

  ~ArcCrashCollectorBridge() override;

  // mojom::CrashCollectorHost overrides.
  void DumpCrash(const std::string& type, mojo::ScopedHandle pipe) override;

  void SetBuildProperties(const std::string& device,
                          const std::string& board,
                          const std::string& cpu_abi) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  std::string device_;
  std::string board_;
  std::string cpu_abi_;

  DISALLOW_COPY_AND_ASSIGN(ArcCrashCollectorBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_
