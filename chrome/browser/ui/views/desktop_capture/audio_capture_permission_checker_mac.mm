// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/audio_capture_permission_checker_mac.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "services/audio/public/cpp/input_ipc.h"

namespace {
BASE_FEATURE(kDesktopMediaPickerCheckAudioPermissions,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

std::unique_ptr<AudioCapturePermissionCheckerMac>
AudioCapturePermissionCheckerMac::MaybeCreate(
    base::RepeatingCallback<void(void)> callback) {
  if (media::IsMacCatapSystemLoopbackCaptureSupported() &&
      base::FeatureList::IsEnabled(kDesktopMediaPickerCheckAudioPermissions)) {
    return std::make_unique<AudioCapturePermissionCheckerMac>(callback);
  }
  return nullptr;
}

AudioCapturePermissionCheckerMac::AudioCapturePermissionCheckerMac(
    base::RepeatingCallback<void(void)> callback)
    : callback_(std::move(callback)) {}

AudioCapturePermissionCheckerMac::~AudioCapturePermissionCheckerMac() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (input_ipc_) {
    input_ipc_->CloseStream();
    input_ipc_.reset();
  }
}

void AudioCapturePermissionCheckerMac::SetAudioStreamFactoryForTest(
    mojo::PendingRemote<media::mojom::AudioStreamFactory>
        audio_stream_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  audio_stream_factory_for_test_ = std::move(audio_stream_factory);
}

AudioCapturePermissionChecker::State
AudioCapturePermissionCheckerMac::GetState() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return audio_permission_state_;
}

void AudioCapturePermissionCheckerMac::RunCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (audio_permission_state_ != State::kUnknown) {
    return;
  }
  RecordUmaAudioCapturePermissionCheckerInteractions(
      AudioCapturePermissionCheckerInteractions::kCheckInitiated);
  audio_permission_state_ = State::kChecking;

  mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory;
  if (audio_stream_factory_for_test_) {
    audio_stream_factory = std::move(audio_stream_factory_for_test_);
  } else {
    content::GetAudioService().BindStreamFactory(
        audio_stream_factory.InitWithNewPipeAndPassReceiver());
  }

  // Reasonable default parameters.
  media::AudioParameters params(
      media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 960);

  input_ipc_ = std::make_unique<audio::InputIPC>(
      std::move(audio_stream_factory),
      media::AudioDeviceDescription::kLoopbackInputDeviceId,
      /*log=*/mojo::NullRemote());
  input_ipc_->CreateStream(this, params,
                           /*automatic_gain_control=*/false,
                           /*total_segments=*/1);
}

void AudioCapturePermissionCheckerMac::OnStreamCreated(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::SyncSocket::ScopedHandle socket_handle,
    bool initially_muted) {
  OnPermissionUpdate(true);
}

void AudioCapturePermissionCheckerMac::OnError(
    media::AudioCapturerSource::ErrorCode code) {
  OnPermissionUpdate(false);
}

void AudioCapturePermissionCheckerMac::OnMuted(bool is_muted) {}

void AudioCapturePermissionCheckerMac::OnIPCClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  input_ipc_.reset();
}

void AudioCapturePermissionCheckerMac::OnPermissionUpdate(bool has_permission) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (input_ipc_) {
    input_ipc_->CloseStream();
    input_ipc_.reset();
  }
  RecordUmaAudioCapturePermissionCheckerInteractions(
      has_permission
          ? AudioCapturePermissionCheckerInteractions::kPermissionGranted
          : AudioCapturePermissionCheckerInteractions::kPermissionDenied);
  audio_permission_state_ = has_permission ? State::kGranted : State::kDenied;
  callback_.Run();
}
