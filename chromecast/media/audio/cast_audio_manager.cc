// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chromecast/media/audio/audio_buildflags.h"
#include "chromecast/media/audio/cast_audio_mixer.h"
#include "chromecast/media/audio/cast_audio_output_stream.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/cma/backend/cma_backend_factory.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/audio/audio_device_description.h"

#if defined(OS_ANDROID)
#include "media/audio/android/audio_track_output_stream.h"
#endif  // defined(OS_ANDROID)

namespace {
// TODO(alokp): Query the preferred value from media backend.
const int kDefaultSampleRate = 48000;

// Define bounds for the output buffer size (in frames).
// TODO(alokp): Query the preferred value from media backend.
static const int kMinimumOutputBufferSize =
    BUILDFLAG(MINIMUM_OUTPUT_BUFFER_SIZE_IN_FRAMES);
static const int kMaximumOutputBufferSize =
    BUILDFLAG(MAXIMUM_OUTPUT_BUFFER_SIZE_IN_FRAMES);
static const int kDefaultOutputBufferSize =
    BUILDFLAG(DEFAULT_OUTPUT_BUFFER_SIZE_IN_FRAMES);

// TODO(jyw): Query the preferred value from media backend.
static const int kDefaultInputBufferSize = 1024;

}  // namespace

namespace chromecast {
namespace media {

CastAudioManager::CastAudioManager(
    std::unique_ptr<::media::AudioThread> audio_thread,
    ::media::AudioLogFactory* audio_log_factory,
    base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
    GetSessionIdCallback get_session_id_callback,
    scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    service_manager::Connector* connector,
    bool use_mixer)
    : CastAudioManager(std::move(audio_thread),
                       audio_log_factory,
                       std::move(backend_factory_getter),
                       std::move(get_session_id_callback),
                       std::move(browser_task_runner),
                       std::move(media_task_runner),
                       connector,
                       use_mixer,
                       false) {}

CastAudioManager::CastAudioManager(
    std::unique_ptr<::media::AudioThread> audio_thread,
    ::media::AudioLogFactory* audio_log_factory,
    base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
    GetSessionIdCallback get_session_id_callback,
    scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    service_manager::Connector* connector,
    bool use_mixer,
    bool force_use_cma_backend_for_output)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      backend_factory_getter_(std::move(backend_factory_getter)),
      get_session_id_callback_(std::move(get_session_id_callback)),
      browser_task_runner_(std::move(browser_task_runner)),
      media_task_runner_(std::move(media_task_runner)),
      browser_connector_(connector),
      force_use_cma_backend_for_output_(force_use_cma_backend_for_output),
      weak_factory_(this) {
  DCHECK(browser_task_runner_->BelongsToCurrentThread());
  DCHECK(backend_factory_getter_);
  DCHECK(get_session_id_callback_);
  DCHECK(browser_connector_);
  weak_this_ = weak_factory_.GetWeakPtr();
  if (use_mixer)
    mixer_ = std::make_unique<CastAudioMixer>(this);
}

CastAudioManager::~CastAudioManager() {
  DCHECK(browser_task_runner_->BelongsToCurrentThread());
}

bool CastAudioManager::HasAudioOutputDevices() {
  return true;
}

void CastAudioManager::GetAudioOutputDeviceNames(
    ::media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  if (HasAudioOutputDevices()) {
    device_names->push_front(::media::AudioDeviceName::CreateCommunications());
    device_names->push_front(::media::AudioDeviceName::CreateDefault());
  }
}

bool CastAudioManager::HasAudioInputDevices() {
  return false;
}

void CastAudioManager::GetAudioInputDeviceNames(
    ::media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  LOG(WARNING) << "No support for input audio devices";
}

::media::AudioParameters CastAudioManager::GetInputStreamParameters(
    const std::string& device_id) {
  LOG(WARNING) << "No support for input audio devices";
  // Need to send a valid AudioParameters object even when it will be unused.
  return ::media::AudioParameters(
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ::media::CHANNEL_LAYOUT_STEREO, kDefaultSampleRate,
      kDefaultInputBufferSize);
}

const char* CastAudioManager::GetName() {
  return "Cast";
}

void CastAudioManager::ReleaseOutputStream(::media::AudioOutputStream* stream) {
  // If |stream| is |mixer_output_stream_|, we should not use
  // AudioManagerBase::ReleaseOutputStream as we do not want the release
  // function to decrement |AudioManagerBase::num_output_streams_|. This is
  // because the stream generated from MakeMixerOutputStream was not created
  // using AudioManagerBase::MakeAudioOutputStream, which appropriately
  // increments this variable.
  if (mixer_output_stream_.get() == stream) {
    DCHECK(mixer_);  // Should only occur if |mixer_| exists
    mixer_output_stream_.reset();
  } else {
    AudioManagerBase::ReleaseOutputStream(stream);
  }
}

CmaBackendFactory* CastAudioManager::cma_backend_factory() {
  if (!cma_backend_factory_)
    cma_backend_factory_ = backend_factory_getter_.Run();
  return cma_backend_factory_;
}

std::string CastAudioManager::GetSessionId(std::string audio_group_id) {
  return get_session_id_callback_.Run(audio_group_id);
}

::media::AudioOutputStream* CastAudioManager::MakeLinearOutputStream(
    const ::media::AudioParameters& params,
    const ::media::AudioManager::LogCallback& log_callback) {
  DCHECK_EQ(::media::AudioParameters::AUDIO_PCM_LINEAR, params.format());

  // If |mixer_| exists, return a mixing stream.
  if (mixer_) {
    return mixer_->MakeStream(params);
  } else {
    return new CastAudioOutputStream(
        this, GetConnector(), params,
        ::media::AudioDeviceDescription::kDefaultDeviceId,
        UseMixerOutputStream(params));
  }
}

::media::AudioOutputStream* CastAudioManager::MakeLowLatencyOutputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id_or_group_id,
    const ::media::AudioManager::LogCallback& log_callback) {
  DCHECK_EQ(::media::AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  // For non-default device IDs, we always want to use CastAudioOutputStream
  // to allow features like redirection. For default device ID, if |mixer_|
  // exists, return a mixing stream. If used, the mixing stream will always use
  // the default device_id.
  if (::media::AudioDeviceDescription::IsDefaultDevice(device_id_or_group_id) &&
      mixer_) {
    return mixer_->MakeStream(params);
  } else {
    return new CastAudioOutputStream(
        this, GetConnector(), params,
        device_id_or_group_id.empty()
            ? ::media::AudioDeviceDescription::kDefaultDeviceId
            : device_id_or_group_id,
        UseMixerOutputStream(params));
  }
}

::media::AudioOutputStream* CastAudioManager::MakeBitstreamOutputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id,
    const ::media::AudioManager::LogCallback& log_callback) {
#if defined(OS_ANDROID)
  DCHECK(params.IsBitstreamFormat());
  return new ::media::AudioTrackOutputStream(this, params);
#else
  NOTREACHED() << " Not implemented on non-android platform.";
  return ::media::AudioManagerBase::MakeBitstreamOutputStream(params, device_id,
                                                              log_callback);
#endif  // defined(OS_ANDROID)
}

::media::AudioInputStream* CastAudioManager::MakeLinearInputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id,
    const ::media::AudioManager::LogCallback& log_callback) {
  LOG(WARNING) << "No support for input audio devices";
  return nullptr;
}

::media::AudioInputStream* CastAudioManager::MakeLowLatencyInputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id,
    const ::media::AudioManager::LogCallback& log_callback) {
  LOG(WARNING) << "No support for input audio devices";
  return nullptr;
}

::media::AudioParameters CastAudioManager::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const ::media::AudioParameters& input_params) {
  ::media::ChannelLayout channel_layout = ::media::CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultOutputBufferSize;
  if (input_params.IsValid()) {
    // Do not change:
    // - the channel layout
    // - the number of bits per sample
    // We support stereo only with 16 bits per sample.
    sample_rate = input_params.sample_rate();
    buffer_size = std::min(
        kMaximumOutputBufferSize,
        std::max(kMinimumOutputBufferSize, input_params.frames_per_buffer()));
  }

  ::media::AudioParameters output_params(
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      sample_rate, buffer_size);
  return output_params;
}

::media::AudioOutputStream* CastAudioManager::MakeMixerOutputStream(
    const ::media::AudioParameters& params) {
  DCHECK(mixer_);
  DCHECK(!mixer_output_stream_);  // Only allow 1 |mixer_output_stream_|.

  // Keep a reference to this stream for proper behavior on
  // CastAudioManager::ReleaseOutputStream.
  mixer_output_stream_.reset(new CastAudioOutputStream(
      this, GetConnector(), params,
      ::media::AudioDeviceDescription::kDefaultDeviceId,
      UseMixerOutputStream(params)));
  return mixer_output_stream_.get();
}

void CastAudioManager::SetConnectorForTesting(
    std::unique_ptr<service_manager::Connector> connector) {
  connector_ = std::move(connector);
}

service_manager::Connector* CastAudioManager::GetConnector() {
  if (!connector_) {
    mojo::PendingReceiver<service_manager::mojom::Connector> receiver;
    connector_ = service_manager::Connector::Create(&receiver);
    browser_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CastAudioManager::BindConnectorReceiver,
                                  weak_this_, std::move(receiver)));
  }
  return connector_.get();
}

void CastAudioManager::BindConnectorReceiver(
    mojo::PendingReceiver<service_manager::mojom::Connector> receiver) {
  browser_connector_->BindConnectorReceiver(std::move(receiver));
}

bool CastAudioManager::UseMixerOutputStream(
    const ::media::AudioParameters& params) {
  bool use_cma_backend =
      (params.effects() & ::media::AudioParameters::MULTIZONE) ||
      !mixer_service::HaveFullMixer() || force_use_cma_backend_for_output_;

  return !use_cma_backend;
}

#if defined(OS_ANDROID)
::media::AudioOutputStream* CastAudioManager::MakeAudioOutputStreamProxy(
    const ::media::AudioParameters& params,
    const std::string& device_id) {
  // Override to use MakeAudioOutputStream to prevent the audio output stream
  // from closing during pause/stop.
  return MakeAudioOutputStream(params, device_id,
                               /*log_callback, not used*/ base::DoNothing());
}
#endif  // defined(OS_ANDROID)

}  // namespace media
}  // namespace chromecast
