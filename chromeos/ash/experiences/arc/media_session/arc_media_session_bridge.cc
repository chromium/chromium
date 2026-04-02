// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/media_session/arc_media_session_bridge.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/experiences/arc/arc_browser_context_keyed_service_factory_base.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "content/public/browser/media_session_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace arc {
namespace {

constexpr char kAudioFocusSourceName[] = "arc";

// ArcAudioFocusManager is an intermediate class that wraps the
// AudioFocusManager and restricts what calls can be made from ARC.
class ArcAudioFocusManager : public media_session::mojom::AudioFocusManager {
 public:
  static mojo::PendingRemote<media_session::mojom::AudioFocusManager> Create() {
    mojo::PendingRemote<media_session::mojom::AudioFocusManager> remote;
    mojo::MakeSelfOwnedReceiver(std::make_unique<ArcAudioFocusManager>(),
                                remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  ArcAudioFocusManager() {
    content::GetMediaSessionService().BindAudioFocusManager(
        audio_focus_remote_.BindNewPipeAndPassReceiver());
    audio_focus_remote_->SetSource(base::UnguessableToken::Create(),
                                   kAudioFocusSourceName);
  }

  ArcAudioFocusManager(const ArcAudioFocusManager&) = delete;
  ArcAudioFocusManager& operator=(const ArcAudioFocusManager&) = delete;

  ~ArcAudioFocusManager() override = default;

  // media_session::mojom::AudioFocusManager:
  void RequestAudioFocus(
      mojo::PendingReceiver<media_session::mojom::AudioFocusRequestClient>
          receiver,
      mojo::PendingRemote<media_session::mojom::MediaSession> session,
      media_session::mojom::MediaSessionInfoPtr session_info,
      media_session::mojom::AudioFocusType type,
      RequestAudioFocusCallback callback) override {
    audio_focus_remote_->RequestAudioFocus(
        std::move(receiver), std::move(session), std::move(session_info), type,
        std::move(callback));
  }

  void RequestGroupedAudioFocus(
      const base::UnguessableToken& request_id,
      mojo::PendingReceiver<media_session::mojom::AudioFocusRequestClient>
          receiver,
      mojo::PendingRemote<media_session::mojom::MediaSession> session,
      media_session::mojom::MediaSessionInfoPtr session_info,
      media_session::mojom::AudioFocusType type,
      const base::UnguessableToken& group_id,
      RequestGroupedAudioFocusCallback callback) override {
    // Grouped audio focus is not allowed from ARC.
    NOTREACHED();
  }

  void GetFocusRequests(GetFocusRequestsCallback callback) override {
    // Getting focus requests is not allowed from ARC.
    NOTREACHED();
  }

  void AddObserver(mojo::PendingRemote<media_session::mojom::AudioFocusObserver>
                       observer) override {
    // Observers are not allowed from ARC.
    NOTREACHED();
  }

  void SetSource(const base::UnguessableToken& identity,
                 const std::string& name) override {
    // Setting the source is not allowed from ARC.
    NOTREACHED();
  }

  void SetEnforcementMode(media_session::mojom::EnforcementMode mode) override {
    // Setting the enforcement mode is not allowed from ARC.
    NOTREACHED();
  }

  void AddSourceObserver(
      const base::UnguessableToken& source_id,
      mojo::PendingRemote<media_session::mojom::AudioFocusObserver> observer)
      override {
    // Source observers are not allowed from ARC.
    NOTREACHED();
  }

  void GetSourceFocusRequests(const base::UnguessableToken& source_id,
                              GetFocusRequestsCallback callback) override {
    // Getting source focus requests is not allowed from ARC.
    NOTREACHED();
  }

  void RequestIdReleased(const base::UnguessableToken& request_id) override {
    // Request ID released is not allowed from ARC.
    NOTREACHED();
  }

  void StartDuckingAllAudio(const std::optional<base::UnguessableToken>&
                                exempted_request_id) override {
    // Ducking all audio is not allowed from ARC.
    NOTREACHED();
  }

  void StopDuckingAllAudio() override {
    // Ducking all audio is not allowed from ARC.
    NOTREACHED();
  }

  void FlushForTesting(FlushForTestingCallback callback) override {
    std::move(callback).Run();
  }

 private:
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
};

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
      media_session::features::kMediaSessionService);
}

}  // namespace

// static
ArcMediaSessionBridge* ArcMediaSessionBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMediaSessionBridgeFactory::GetForBrowserContext(context);
}

// static
ArcMediaSessionBridge* ArcMediaSessionBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcMediaSessionBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcMediaSessionBridge::ArcMediaSessionBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_observation_.Observe(arc_bridge_service_->media_session());
}

ArcMediaSessionBridge::~ArcMediaSessionBridge() = default;

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
  if (!ms_instance) {
    return;
  }

  if (!IsArcUnifiedAudioFocusEnabled()) {
    DVLOG(2) << "ArcMediaSessionBridge will disable audio focus";
    ms_instance->DisableAudioFocus();
    return;
  }

  DVLOG(2) << "ArcMediaSessionBridge will enable audio focus";
  ms_instance->EnableAudioFocus(ArcAudioFocusManager::Create());
}

// static
void ArcMediaSessionBridge::EnsureFactoryBuilt() {
  ArcMediaSessionBridgeFactory::GetInstance();
}

}  // namespace arc
