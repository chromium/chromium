// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_device_factory.h"

#include <string>

#include "base/android/bundle_utils.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/media/audio/cast_audio_output_device.h"
#include "content/public/renderer/render_frame.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/output_device_info.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/modules/media/audio/audio_output_ipc_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {
namespace media {

namespace {

constexpr base::TimeDelta kAuthorizationTimeout = base::Seconds(100);

content::RenderFrame* GetRenderFrameForToken(
    const blink::LocalFrameToken& frame_token) {
  auto* web_frame = blink::WebLocalFrame::FromFrameToken(frame_token);
  DCHECK(web_frame);

  return content::RenderFrame::FromWebFrame(web_frame);
}

scoped_refptr<::media::AudioOutputDevice> NewOutputDevice(
    const blink::LocalFrameToken& frame_token,
    const ::media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  auto device = base::MakeRefCounted<::media::AudioOutputDevice>(
      blink::AudioOutputIPCFactory::GetInstance().CreateAudioOutputIPC(
          frame_token),
      blink::AudioOutputIPCFactory::GetInstance().io_task_runner(), params,
      auth_timeout);
  device->RequestDeviceAuthorization();
  return device;
}

}  // namespace

class NonSwitchableAudioRendererSink
    : public ::media::SwitchableAudioRendererSink {
 public:
  explicit NonSwitchableAudioRendererSink(
      const blink::LocalFrameToken& frame_token,
      const ::media::AudioSinkParameters& params)
      : frame_token_(frame_token), sink_params_(params) {
    auto* render_frame = GetRenderFrameForToken(frame_token);
    DCHECK(render_frame);
    render_frame->GetBrowserInterfaceBroker()->GetInterface(
        audio_socket_broker_.InitWithNewPipeAndPassReceiver());
    render_frame->GetBrowserInterfaceBroker()->GetInterface(
        app_media_info_manager_.InitWithNewPipeAndPassReceiver());
  }

  void Initialize(const ::media::AudioParameters& params,
                  RenderCallback* callback) override {
    // NonSwitchableAudioRendererSink derives from RestartableRenderSink which
    // does allow calling Initialize and Play again after stopping.
    if (is_initialized_)
      return;
    is_initialized_ = true;
    if (!(base::android::BundleUtils::IsBundle() ||
          base::FeatureList::IsEnabled(kEnableCastAudioOutputDevice)) ||
        params.IsBitstreamFormat()) {
      output_device_ =
          NewOutputDevice(frame_token_, sink_params_, kAuthorizationTimeout);
    } else {
      output_device_ = base::MakeRefCounted<CastAudioOutputDevice>(
          std::move(audio_socket_broker_), std::move(app_media_info_manager_));
    }
    output_device_->Initialize(params, callback);
  }

  void Start() override {
    DCHECK(output_device_);
    output_device_->Start();
  }

  void Stop() override {
    if (output_device_)
      output_device_->Stop();
  }

  void Pause() override {
    DCHECK(output_device_);
    output_device_->Pause();
  }

  void Play() override {
    DCHECK(output_device_);
    output_device_->Play();
  }

  bool SetVolume(double volume) override {
    DCHECK(output_device_);
    return output_device_->SetVolume(volume);
  }

  ::media::OutputDeviceInfo GetOutputDeviceInfo() override {
    if (output_device_) {
      return output_device_->GetOutputDeviceInfo();
    }
    // GetOutputDeviceInfo() may be called when the underlying `output_device_`
    // hasn't been constructed. Return the default set of parameters in this
    // case.
    return ::media::OutputDeviceInfo(
        std::string(), ::media::OUTPUT_DEVICE_STATUS_OK,
        ::media::AudioParameters(
            ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
            ::media::ChannelLayoutConfig::Stereo(), 48000, 480));
  }

  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override {
    if (output_device_) {
      output_device_->GetOutputDeviceInfoAsync(std::move(info_cb));
      return;
    }
    // Always post to avoid the caller being reentrant.
    ::media::BindToCurrentLoop(
        base::BindOnce(std::move(info_cb), GetOutputDeviceInfo()))
        .Run();
  }

  bool IsOptimizedForHardwareParameters() override {
    if (output_device_) {
      return output_device_->IsOptimizedForHardwareParameters();
    }
    return false;
  }

  bool CurrentThreadIsRenderingThread() override {
    return output_device_ && output_device_->CurrentThreadIsRenderingThread();
  }

  void SwitchOutputDevice(const std::string& device_id,
                          ::media::OutputDeviceStatusCB callback) override {
    LOG(ERROR) << __func__ << " is not suported.";
    std::move(callback).Run(::media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  }

  void Flush() override {
    DCHECK(output_device_);
    output_device_->Flush();
  }

 protected:
  ~NonSwitchableAudioRendererSink() override {
    if (output_device_)
      output_device_->Stop();
  }

 private:
  const blink::LocalFrameToken frame_token_;
  const ::media::AudioSinkParameters sink_params_;

  mojo::PendingRemote<mojom::AudioSocketBroker> audio_socket_broker_;
  mojo::PendingRemote<::media::mojom::CastApplicationMediaInfoManager>
      app_media_info_manager_;
  scoped_refptr<::media::AudioRendererSink> output_device_;
  bool is_initialized_ = false;
};

CastAudioDeviceFactory::CastAudioDeviceFactory() {
  DVLOG(1) << "Register CastAudioDeviceFactory";
}

CastAudioDeviceFactory::~CastAudioDeviceFactory() {
  DVLOG(1) << "Unregister CastAudioDeviceFactory";
}

scoped_refptr<::media::SwitchableAudioRendererSink>
CastAudioDeviceFactory::NewSwitchableAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const ::media::AudioSinkParameters& params) {
  return base::MakeRefCounted<NonSwitchableAudioRendererSink>(frame_token,
                                                              params);
}

}  // namespace media
}  // namespace chromecast
