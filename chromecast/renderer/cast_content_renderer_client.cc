// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chromecast/base/bitstream_audio_codecs.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/common/cors_exempt_headers.h"
#include "chromecast/crash/app_state_tracker.h"
#include "chromecast/media/base/media_codec_support.h"
#include "chromecast/media/base/supported_codec_profile_levels_memo.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/renderer/cast_url_loader_throttle_provider.h"
#include "chromecast/renderer/cast_websocket_handshake_throttle_provider.h"
#include "chromecast/renderer/media/key_systems_cast.h"
#include "chromecast/renderer/media/media_caps_observer_impl.h"
#include "components/cast_receiver/renderer/public/content_renderer_client_mixins.h"
#include "components/media_control/renderer/media_playback_options.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/on_load_script_injector/renderer/on_load_script_injector.h"
#include "components/url_rewrite/common/url_request_rewrite_rules.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/audio_parameters.h"
#include "media/base/key_system_info.h"
#include "media/base/media.h"
#include "media/base/remoting_constants.h"
#include "media/remoting/receiver_controller.h"
#include "media/remoting/stream_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/bundle_utils.h"
#include "chromecast/media/audio/cast_audio_device_factory.h"
#include "components/cdm/renderer/key_system_support_update.h"
#include "media/base/android/media_codec_util.h"
#else
#include "chromecast/renderer/memory_pressure_observer_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace chromecast {
namespace shell {
namespace {
bool IsSupportedBitstreamAudioCodecHelper(::media::AudioCodec codec, int mask) {
  return (codec == ::media::AudioCodec::kAC3 &&
          (kBitstreamAudioCodecAc3 & mask)) ||
         (codec == ::media::AudioCodec::kEAC3 &&
          (kBitstreamAudioCodecEac3 & mask)) ||
         (codec == ::media::AudioCodec::kDTS &&
          (kBitstreamAudioCodecDts & mask)) ||
         (codec == ::media::AudioCodec::kDTSXP2 &&
          (kBitstreamAudioCodecDtsXP2 & mask)) ||
         (codec == ::media::AudioCodec::kMpegHAudio &&
          (kBitstreamAudioCodecMpegHAudio & mask));
}
}  // namespace

#if BUILDFLAG(IS_ANDROID)
// Audio renderer algorithm maximum capacity. 5s buffer is already large enough,
// we don't need a larger capacity. Otherwise audio renderer will double the
// buffer size when underrun happens, which will cause the playback paused to
// wait long time for enough buffers.
constexpr base::TimeDelta kAudioRendererMaxCapacity = base::Seconds(5);
// Audio renderer algorithm starting capacity.  Configure large enough to
// prevent underrun.
constexpr base::TimeDelta kAudioRendererStartingCapacity = base::Seconds(5);
constexpr base::TimeDelta kAudioRendererStartingCapacityEncrypted =
    base::Seconds(5);
#endif  // BUILDFLAG(IS_ANDROID)

CastContentRendererClient::CastContentRendererClient()
    : cast_receiver_mixins_(cast_receiver::ContentRendererClientMixins::Create(
          base::BindRepeating(&IsCorsExemptHeader))),
      supported_profiles_(
          std::make_unique<media::SupportedCodecProfileLevelsMemo>()),
      activity_url_filter_manager_(
          std::make_unique<CastActivityUrlFilterManager>()) {
#if BUILDFLAG(IS_ANDROID)
  // Registers a custom content::AudioDeviceFactory
  cast_audio_device_factory_ =
      std::make_unique<media::CastAudioDeviceFactory>();
#endif  // BUILDFLAG(IS_ANDROID)
}

CastContentRendererClient::~CastContentRendererClient() = default;

void CastContentRendererClient::RenderThreadStarted() {
  // Register as observer for media capabilities
  content::RenderThread* thread = content::RenderThread::Get();
  mojo::Remote<media::mojom::MediaCaps> media_caps;
  thread->BindHostReceiver(media_caps.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<media::mojom::MediaCapsObserver> proxy;
  media_caps_observer_.reset(
      new media::MediaCapsObserverImpl(&proxy, supported_profiles_.get()));
  media_caps->AddObserver(std::move(proxy));

#if !BUILDFLAG(IS_ANDROID)
  // Register to observe memory pressure changes
  mojo::Remote<chromecast::mojom::MemoryPressureController>
      memory_pressure_controller;
  thread->BindHostReceiver(
      memory_pressure_controller.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<chromecast::mojom::MemoryPressureObserver>
      memory_pressure_proxy;
  memory_pressure_observer_.reset(
      new MemoryPressureObserverImpl(&memory_pressure_proxy));
  memory_pressure_controller->AddObserver(std::move(memory_pressure_proxy));
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string last_launched_app =
      command_line->GetSwitchValueNative(switches::kLastLaunchedApp);
  if (!last_launched_app.empty())
    AppStateTracker::SetLastLaunchedApp(last_launched_app);

  std::string previous_app =
      command_line->GetSwitchValueNative(switches::kPreviousApp);
  if (!previous_app.empty())
    AppStateTracker::SetPreviousApp(previous_app);
}

void CastContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  DCHECK(render_frame);

  cast_receiver_mixins_->RenderFrameCreated(*render_frame);

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  if (render_frame->IsMainFrame()) {
    main_frame_feature_manager_on_associated_interface_ =
        new FeatureManagerOnAssociatedInterface(render_frame);
  } else {
    new FeatureManagerOnAssociatedInterface(render_frame);
  }

  if (!app_media_capabilities_observer_receiver_.is_bound()) {
    mojo::Remote<mojom::ApplicationMediaCapabilities> app_media_capabilities;
    render_frame->GetBrowserInterfaceBroker().GetInterface(
        app_media_capabilities.BindNewPipeAndPassReceiver());
    app_media_capabilities->AddObserver(
        app_media_capabilities_observer_receiver_.BindNewPipeAndPassRemote());
  }

  activity_url_filter_manager_->OnRenderFrameCreated(render_frame);
}

void CastContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {}

void CastContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {}

std::unique_ptr<::media::KeySystemSupportRegistration>
CastContentRendererClient::GetSupportedKeySystems(
    content::RenderFrame* render_frame,
    ::media::GetSupportedKeySystemsCB cb) {
#if BUILDFLAG(IS_ANDROID)
  return cdm::GetSupportedKeySystemsUpdates(render_frame,
                                            /*can_persist_data=*/true,
                                            std::move(cb));
#else
  ::media::KeySystemInfos key_systems;
  media::AddChromecastKeySystems(&key_systems,
                                 false /* enable_persistent_license_support */);
  std::move(cb).Run(std::move(key_systems));
  return nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool CastContentRendererClient::IsSupportedAudioType(
    const ::media::AudioType& type) {
#if BUILDFLAG(IS_ANDROID)
  if (type.spatial_rendering)
    return false;

  // No ATV device we know of has (E)AC3 decoder, so it relies on the audio sink
  // device.
  if (type.codec == ::media::AudioCodec::kEAC3) {
    return kBitstreamAudioCodecEac3 &
           supported_bitstream_audio_codecs_info_.codecs;
  }
  if (type.codec == ::media::AudioCodec::kAC3) {
    return kBitstreamAudioCodecAc3 &
           supported_bitstream_audio_codecs_info_.codecs;
  }
  if (type.codec == ::media::AudioCodec::kDTS) {
    return kBitstreamAudioCodecDts &
           supported_bitstream_audio_codecs_info_.codecs;
  }
  if (type.codec == ::media::AudioCodec::kDTSXP2) {
    return kBitstreamAudioCodecDtsXP2 &
           supported_bitstream_audio_codecs_info_.codecs;
  }
  if (type.codec == ::media::AudioCodec::kMpegHAudio) {
    return kBitstreamAudioCodecMpegHAudio &
           supported_bitstream_audio_codecs_info_.codecs;
  }

  return ::media::IsDefaultSupportedAudioType(type);
#else
  if (type.profile == ::media::AudioCodecProfile::kXHE_AAC)
    return false;

  // If the HDMI sink supports bitstreaming the codec, then the vendor backend
  // does not need to support it.
  if (CheckSupportedBitstreamAudioCodec(type.codec, type.spatial_rendering))
    return true;

  media::AudioCodec codec = media::ToCastAudioCodec(type.codec);
  // Cast platform implements software decoding of Opus and FLAC, so only PCM
  // support is necessary in order to support Opus and FLAC.
  if (codec == media::kCodecOpus || codec == media::kCodecFLAC)
    codec = media::kCodecPCM;

  media::AudioConfig cast_audio_config;
  cast_audio_config.codec = codec;
  return media::MediaCapabilitiesShlib::IsSupportedAudioConfig(
      cast_audio_config);
#endif
}

bool CastContentRendererClient::IsSupportedVideoType(
    const ::media::VideoType& type) {
  // TODO(servolk): make use of eotf.

  // TODO(crbug.com/40124585): Check attached screen for support of
  // type.hdr_metadata_type.
  if (type.hdr_metadata_type != ::gfx::HdrMetadataType::kNone) {
    NOTIMPLEMENTED() << "HdrMetadataType support signaling not implemented.";
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  return supported_profiles_->IsSupportedVideoConfig(
      media::ToCastVideoCodec(type.codec, type.profile),
      media::ToCastVideoProfile(type.profile), type.level);
#else
  return media::MediaCapabilitiesShlib::IsSupportedVideoConfig(
      media::ToCastVideoCodec(type.codec, type.profile),
      media::ToCastVideoProfile(type.profile), type.level);
#endif
}

bool CastContentRendererClient::IsSupportedBitstreamAudioCodec(
    ::media::AudioCodec codec) {
  return IsSupportedBitstreamAudioCodecHelper(
      codec, supported_bitstream_audio_codecs_info_.codecs);
}

bool CastContentRendererClient::CheckSupportedBitstreamAudioCodec(
    ::media::AudioCodec codec,
    bool check_spatial_rendering) {
  if (!IsSupportedBitstreamAudioCodec(codec))
    return false;

  if (!check_spatial_rendering)
    return true;

  return IsSupportedBitstreamAudioCodecHelper(
      codec, supported_bitstream_audio_codecs_info_.spatial_rendering);
}

std::unique_ptr<blink::WebPrescientNetworking>
CastContentRendererClient::CreatePrescientNetworking(
    content::RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

bool CastContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    bool render_frame_has_played_media_before,
    base::OnceClosure closure) {
  return cast_receiver_mixins_->DeferMediaLoad(*render_frame,
                                               std::move(closure));
}

std::unique_ptr<::media::Demuxer>
CastContentRendererClient::OverrideDemuxerForUrl(
    content::RenderFrame* render_frame,
    const GURL& url,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (render_frame->GetRenderFrameMediaPlaybackOptions()
          .is_remoting_renderer_enabled() &&
      url.SchemeIs(::media::remoting::kRemotingScheme)) {
    return std::make_unique<::media::remoting::StreamProvider>(
        ::media::remoting::ReceiverController::GetInstance(), task_runner);
  }
  return nullptr;
}

bool CastContentRendererClient::IsIdleMediaSuspendEnabled() {
  return false;
}

void CastContentRendererClient::
    SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  // Allow HtmlMediaElement.volume to be greater than 1, for normalization.
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "MediaElementVolumeGreaterThanOne", true);
  // Settings for ATV (Android defaults are not what we want).
  blink::WebRuntimeFeatures::EnableMediaControlsOverlayPlayButton(false);
}

void CastContentRendererClient::OnSupportedBitstreamAudioCodecsChanged(
    const BitstreamAudioCodecsInfo& info) {
  supported_bitstream_audio_codecs_info_ = info;
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
CastContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<CastWebSocketHandshakeThrottleProvider>(
      activity_url_filter_manager_.get());
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
CastContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type) {
  auto throttle_provider = std::make_unique<CastURLLoaderThrottleProvider>(
      type, activity_url_filter_manager());
  return cast_receiver_mixins_->ExtendURLLoaderThrottleProvider(
      std::move(throttle_provider));
}

std::optional<::media::AudioRendererAlgorithmParameters>
CastContentRendererClient::GetAudioRendererAlgorithmParameters(
    ::media::AudioParameters audio_parameters) {
#if BUILDFLAG(IS_ANDROID)
  ::media::AudioRendererAlgorithmParameters parameters;
  parameters.max_capacity = kAudioRendererMaxCapacity;
  parameters.starting_capacity = kAudioRendererStartingCapacity;
  parameters.starting_capacity_for_encrypted =
      kAudioRendererStartingCapacityEncrypted;
  return std::optional<::media::AudioRendererAlgorithmParameters>(parameters);
#else
  return std::nullopt;
#endif
}

}  // namespace shell
}  // namespace chromecast
