// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/mojom/application_media_capabilities.mojom.h"
#include "chromecast/renderer/cast_activity_url_filter_manager.h"
#include "chromecast/renderer/feature_manager_on_associated_interface.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/key_systems_support_registration.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace cast_receiver {
class ContentRendererClientMixins;
}  // namespace cast_receiver

namespace chromecast {
class MemoryPressureObserverImpl;
namespace media {
class MediaCapsObserverImpl;
class SupportedCodecProfileLevelsMemo;

#if BUILDFLAG(IS_ANDROID)
class CastAudioDeviceFactory;
#endif  // BUILDFLAG(IS_ANDROID)
}

namespace shell {

class CastContentRendererClient
    : public content::ContentRendererClient,
      public mojom::ApplicationMediaCapabilitiesObserver {
 public:
  // Creates an implementation of CastContentRendererClient. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastContentRendererClient> Create();

  CastContentRendererClient(const CastContentRendererClient&) = delete;
  CastContentRendererClient& operator=(const CastContentRendererClient&) =
      delete;

  ~CastContentRendererClient() override;

  // ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  std::unique_ptr<::media::KeySystemSupportRegistration> GetSupportedKeySystems(
      content::RenderFrame* render_frame,
      ::media::GetSupportedKeySystemsCB cb) override;
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
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;
  bool IsIdleMediaSuspendEnabled() override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider() override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType type) override;
  std::optional<::media::AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(
      ::media::AudioParameters audio_parameters) override;

 protected:
  CastContentRendererClient();

  CastActivityUrlFilterManager* activity_url_filter_manager() {
    return activity_url_filter_manager_.get();
  }

  // TODO(guohuideng): Move |feature_manager_on_associated_interface_| to
  // private when we can.
  FeatureManagerOnAssociatedInterface*
      main_frame_feature_manager_on_associated_interface_{nullptr};

 private:
  // mojom::ApplicationMediaCapabilitiesObserver implementation:
  void OnSupportedBitstreamAudioCodecsChanged(
      const BitstreamAudioCodecsInfo& info) override;

  bool CheckSupportedBitstreamAudioCodec(::media::AudioCodec codec,
                                         bool check_spatial_rendering);

  std::unique_ptr<cast_receiver::ContentRendererClientMixins>
      cast_receiver_mixins_;

  std::unique_ptr<media::MediaCapsObserverImpl> media_caps_observer_;
  std::unique_ptr<media::SupportedCodecProfileLevelsMemo> supported_profiles_;
  mojo::Receiver<mojom::ApplicationMediaCapabilitiesObserver>
      app_media_capabilities_observer_receiver_{this};
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<MemoryPressureObserverImpl> memory_pressure_observer_;
#endif

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<media::CastAudioDeviceFactory> cast_audio_device_factory_;
#endif

  BitstreamAudioCodecsInfo supported_bitstream_audio_codecs_info_;

  std::unique_ptr<CastActivityUrlFilterManager> activity_url_filter_manager_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
