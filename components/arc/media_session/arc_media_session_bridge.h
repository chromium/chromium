// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_MEDIA_SESSION_ARC_MEDIA_SESSION_BRIDGE_H_
#define COMPONENTS_ARC_MEDIA_SESSION_ARC_MEDIA_SESSION_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "components/arc/mojom/media_session.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// ArcMediaSessionBridge exposes the media session service to ARC. This allows
// Android apps to request and manage audio focus using the internal Chrome
// API. This means that audio focus management is unified across both Android
// and Chrome.
class ArcMediaSessionBridge
    : public KeyedService,
      public ConnectionObserver<mojom::MediaSessionInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMediaSessionBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcMediaSessionBridge(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);
  ~ArcMediaSessionBridge() override;

  // ConnectionObserver<mojom::MediaSessionInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

 private:
  void SetupAudioFocus();

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  DISALLOW_COPY_AND_ASSIGN(ArcMediaSessionBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_MEDIA_SESSION_ARC_MEDIA_SESSION_BRIDGE_H_
