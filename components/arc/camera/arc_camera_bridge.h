// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_CAMERA_ARC_CAMERA_BRIDGE_H_
#define COMPONENTS_ARC_CAMERA_ARC_CAMERA_BRIDGE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "components/arc/mojom/camera.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles Camera-related requests from the ARC container.
class ArcCameraBridge : public KeyedService, public mojom::CameraHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcCameraBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcCameraBridge(content::BrowserContext* context,
                  ArcBridgeService* bridge_service);
  ~ArcCameraBridge() override;

  // mojom::CameraHost overrides:
  void StartCameraService(StartCameraServiceCallback callback) override;
  void RegisterCameraHalClient(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client) override;

 private:
  class PendingStartCameraServiceResult;

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  std::map<PendingStartCameraServiceResult*,
           std::unique_ptr<PendingStartCameraServiceResult>>
      pending_start_camera_service_results_;

  DISALLOW_COPY_AND_ASSIGN(ArcCameraBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_CAMERA_ARC_CAMERA_BRIDGE_H_
