// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/media_session/arc_media_session_bridge.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace arc {
namespace {

constexpr char kAudioFocusSourceName[] = "arc";

// Singleton factory for ArcAccessibilityHelperBridge.
class ArcMediaSessionBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMediaSessionBridge,
          ArcMediaSessionBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMediaSessionBridgeFactory";

  static ArcMediaSessionBridgeFactory* GetInstance() {
    static base::NoDestructor<ArcMediaSessionBridgeFactory> factory;
    return factory.get();
  }

  ArcMediaSessionBridgeFactory() = default;
  ~ArcMediaSessionBridgeFactory() override = default;
};

bool IsArcUnifiedAudioFocusEnabled() {
  return base::FeatureList::IsEnabled(
             media_session::features::kMediaSessionService) &&
         base::FeatureList::IsEnabled(kEnableUnifiedAudioFocusFeature);
}

}  // namespace

// static
ArcMediaSessionBridge* ArcMediaSessionBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMediaSessionBridgeFactory::GetForBrowserContext(context);
}

ArcMediaSessionBridge::ArcMediaSessionBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->media_session()->AddObserver(this);
}

ArcMediaSessionBridge::~ArcMediaSessionBridge() {
  arc_bridge_service_->media_session()->RemoveObserver(this);
}

void ArcMediaSessionBridge::OnConnectionReady() {
  DVLOG(2) << "ArcMediaSessionBridge::OnConnectionReady";
  SetupAudioFocus();
}

void ArcMediaSessionBridge::OnConnectionClosed() {
  DVLOG(2) << "ArcMediaSessionBridge::OnConnectionClosed";
}

void ArcMediaSessionBridge::SetupAudioFocus() {
  DVLOG(2) << "ArcMediaSessionBridge::SetupAudioFocus";
  mojom::MediaSessionInstance* ms_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->media_session(), DisableAudioFocus);
  if (!ms_instance)
    return;

  if (!IsArcUnifiedAudioFocusEnabled()) {
    DVLOG(2) << "ArcMediaSessionBridge will disable audio focus";
    ms_instance->DisableAudioFocus();
    return;
  }

  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus;
  content::GetSystemConnector()->Connect(
      media_session::mojom::kServiceName,
      audio_focus.BindNewPipeAndPassReceiver());

  audio_focus->SetSource(base::UnguessableToken::Create(),
                         kAudioFocusSourceName);

  DVLOG(2) << "ArcMediaSessionBridge will enable audio focus";
  ms_instance->EnableAudioFocus(audio_focus.Unbind());
}

}  // namespace arc
