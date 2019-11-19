// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_device_factory.h"

#include <string>

#include "content/renderer/media/audio/audio_output_ipc_factory.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/output_device_info.h"

namespace chromecast {
namespace media {

class NonSwitchableAudioRendererSink
    : public ::media::SwitchableAudioRendererSink {
 public:
  explicit NonSwitchableAudioRendererSink(
      scoped_refptr<::media::AudioOutputDevice> output_device)
      : output_device_(std::move(output_device)), is_initialized_(false) {}

  void Initialize(const ::media::AudioParameters& params,
                  RenderCallback* callback) override {
    // NonSwitchableAudioRendererSink derives from RestartableRenderSink which
    // does allow calling Initialize and Play again after stopping.
    if (is_initialized_)
      return;
    is_initialized_ = true;
    output_device_->Initialize(params, callback);
  }

  void Start() override { output_device_->Start(); }

  void Stop() override { output_device_->Stop(); }

  void Pause() override { output_device_->Pause(); }

  void Play() override { output_device_->Play(); }

  bool SetVolume(double volume) override {
    return output_device_->SetVolume(volume);
  }

  ::media::OutputDeviceInfo GetOutputDeviceInfo() override {
    return output_device_->GetOutputDeviceInfo();
  }

  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override {
    output_device_->GetOutputDeviceInfoAsync(std::move(info_cb));
  }

  bool IsOptimizedForHardwareParameters() override {
    return output_device_->IsOptimizedForHardwareParameters();
  }

  bool CurrentThreadIsRenderingThread() override {
    return output_device_->CurrentThreadIsRenderingThread();
  }

  void SwitchOutputDevice(const std::string& device_id,
                          ::media::OutputDeviceStatusCB callback) override {
    LOG(ERROR) << __func__ << " is not suported.";
    std::move(callback).Run(::media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  }

  void Flush() override { output_device_->Flush(); }

 protected:
  ~NonSwitchableAudioRendererSink() override { output_device_->Stop(); }

 private:
  scoped_refptr<::media::AudioOutputDevice> output_device_;
  bool is_initialized_;
};

scoped_refptr<::media::AudioOutputDevice> NewOutputDevice(
    int render_frame_id,
    const ::media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  auto device = base::MakeRefCounted<::media::AudioOutputDevice>(
      content::AudioOutputIPCFactory::get()->CreateAudioOutputIPC(
          render_frame_id),
      content::AudioOutputIPCFactory::get()->io_task_runner(), params,
      auth_timeout);
  device->RequestDeviceAuthorization();
  return device;
}

CastAudioDeviceFactory::CastAudioDeviceFactory()
    : content::AudioDeviceFactory() {
  DVLOG(1) << "Register CastAudioDeviceFactory";
}

CastAudioDeviceFactory::~CastAudioDeviceFactory() {
  DVLOG(1) << "Unregister CastAudioDeviceFactory";
}

scoped_refptr<::media::AudioRendererSink>
CastAudioDeviceFactory::CreateFinalAudioRendererSink(
    int render_frame_id,
    const ::media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  // Use default implementation.
  return nullptr;
}

scoped_refptr<::media::AudioRendererSink>
CastAudioDeviceFactory::CreateAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    int render_frame_id,
    const ::media::AudioSinkParameters& params) {
  // Use default implementation.
  return nullptr;
}

scoped_refptr<::media::SwitchableAudioRendererSink>
CastAudioDeviceFactory::CreateSwitchableAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    int render_frame_id,
    const ::media::AudioSinkParameters& params) {
  return base::MakeRefCounted<NonSwitchableAudioRendererSink>(NewOutputDevice(
      render_frame_id, params, base::TimeDelta::FromSeconds(100)));
}

scoped_refptr<::media::AudioCapturerSource>
CastAudioDeviceFactory::CreateAudioCapturerSource(
    int render_frame_id,
    const ::media::AudioSourceParameters& params) {
  // Use default implementation.
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
