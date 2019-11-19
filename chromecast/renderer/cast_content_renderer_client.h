// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/mojom/application_media_capabilities.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace extensions {
class ExtensionsClient;
class ExtensionsGuestViewContainerDispatcher;
class CastExtensionsRendererClient;
}  // namespace extensions

namespace network_hints {
class WebPrescientNetworkingImpl;
}  // namespace network_hints

namespace chromecast {
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
      public mojom::ApplicationMediaCapabilitiesObserver {
 public:
  // Creates an implementation of CastContentRendererClient. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastContentRendererClient> Create();

  ~CastContentRendererClient() override;

  // ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderViewCreated(content::RenderView* render_view) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  content::BrowserPluginDelegate* CreateBrowserPluginDelegate(
      content::RenderFrame* render_frame,
      const content::WebPluginInfo& info,
      const std::string& mime_type,
      const GURL& original_url) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>*
          key_systems_properties) override;
  bool IsSupportedAudioType(const ::media::AudioType& type) override;
  bool IsSupportedVideoType(const ::media::VideoType& type) override;
  bool IsSupportedBitstreamAudioCodec(::media::AudioCodec codec) override;
  blink::WebPrescientNetworking* GetPrescientNetworking() override;
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool render_frame_has_played_media_before,
                      base::OnceClosure closure) override;
  bool IsIdleMediaSuspendEnabled() override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
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

 private:
  // mojom::ApplicationMediaCapabilitiesObserver implementation:
  void OnSupportedBitstreamAudioCodecsChanged(int codecs) override;

  std::unique_ptr<network_hints::WebPrescientNetworkingImpl>
      web_prescient_networking_impl_;
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

  int supported_bitstream_audio_codecs_;

  DISALLOW_COPY_AND_ASSIGN(CastContentRendererClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
