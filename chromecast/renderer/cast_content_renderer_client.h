// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/mojom/application_media_capabilities.mojom.h"
#include "chromecast/renderer/cast_activity_url_filter_manager.h"
#include "chromecast/renderer/identification_settings_manager_store.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace extensions {
class ExtensionsClient;
class ExtensionsGuestViewContainerDispatcher;
class CastExtensionsRendererClient;
}  // namespace extensions

namespace chromecast {
class IdentificationSettingsManager;
class MemoryPressureObserverImpl;
namespace media {
class MediaCapsObserverImpl;
class SupportedCodecProfileLevelsMemo;

#if defined(OS_ANDROID)
class CastAudioDeviceFactory;
#endif  // defined(OS_ANDROID)
}

namespace shell {

class CastContentRendererClient
    : public content::ContentRendererClient,
      public mojom::ApplicationMediaCapabilitiesObserver,
      public IdentificationSettingsManagerStore {
 public:
  // Creates an implementation of CastContentRendererClient. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastContentRendererClient> Create();

  ~CastContentRendererClient() override;

  // ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderViewCreated(content::RenderView* render_view) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>*
          key_systems_properties) override;
  bool IsSupportedAudioType(const ::media::AudioType& type) override;
  bool IsSupportedVideoType(const ::media::VideoType& type) override;
  bool IsSupportedBitstreamAudioCodec(::media::AudioCodec codec) override;
  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking(
      content::RenderFrame* render_frame) override;
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool render_frame_has_played_media_before,
                      base::OnceClosure closure) override;
  std::unique_ptr<::media::Demuxer> OverrideDemuxerForUrl(
      content::RenderFrame* render_frame,
      const GURL& url,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  bool IsIdleMediaSuspendEnabled() override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider() override;
  std::unique_ptr<content::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      content::URLLoaderThrottleProviderType type) override;
  base::Optional<::media::AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(
      ::media::AudioParameters audio_parameters) override;

 protected:
  CastContentRendererClient();

  // Returns true if running is deferred until in foreground; false if running
  // occurs immediately.
  virtual bool RunWhenInForeground(content::RenderFrame* render_frame,
                                   base::OnceClosure closure);

  CastActivityUrlFilterManager* activity_url_filter_manager() {
    return activity_url_filter_manager_.get();
  }

 private:
  // mojom::ApplicationMediaCapabilitiesObserver implementation:
  void OnSupportedBitstreamAudioCodecsChanged(
      const BitstreamAudioCodecsInfo& info) override;

  bool CheckSupportedBitstreamAudioCodec(::media::AudioCodec codec,
                                         bool check_spatial_rendering);

  // IdentificationSettingsManagerStore implementation:
  IdentificationSettingsManager* GetSettingsManagerFromRenderFrameID(
      int render_frame_id) override;

  // Called when a render frame is removed.
  void OnRenderFrameRemoved(int render_frame_id);

  std::unique_ptr<media::MediaCapsObserverImpl> media_caps_observer_;
  std::unique_ptr<media::SupportedCodecProfileLevelsMemo> supported_profiles_;
  mojo::Receiver<mojom::ApplicationMediaCapabilitiesObserver>
      app_media_capabilities_observer_receiver_{this};
#if !defined(OS_ANDROID)
  std::unique_ptr<MemoryPressureObserverImpl> memory_pressure_observer_;
#endif

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::CastExtensionsRendererClient>
      extensions_renderer_client_;
  std::unique_ptr<extensions::ExtensionsGuestViewContainerDispatcher>
      guest_view_container_dispatcher_;
#endif

#if defined(OS_ANDROID)
  std::unique_ptr<media::CastAudioDeviceFactory> cast_audio_device_factory_;
#endif

  BitstreamAudioCodecsInfo supported_bitstream_audio_codecs_info_;

  base::flat_map<int, std::unique_ptr<IdentificationSettingsManager>>
      settings_managers_;
  std::unique_ptr<CastActivityUrlFilterManager> activity_url_filter_manager_;

  DISALLOW_COPY_AND_ASSIGN(CastContentRendererClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
