// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_service.h"

#include <dlfcn.h>
#include <sys/resource.h>

#include "base/files/file_util.h"
#include "chromeos/services/tts/constants.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "services/audio/public/cpp/output_device.h"

namespace chromeos {
namespace tts {

// Simple helper to bridge logging in the shared library to Chrome's logging.
void HandleLibraryLogging(int severity, const char* message) {
  switch (severity) {
    case logging::LOG_INFO:
      // Suppressed.
      break;
    case logging::LOG_WARNING:
      LOG(WARNING) << message;
      break;
    case logging::LOG_ERROR:
      LOG(ERROR) << message;
      break;
    default:
      break;
  }
}

// TtsService is mostly glue code that adapts the TtsStream interface into a
// form needed by libchrometts.so. As is convention with shared objects, the
// lifetime of all arguments passed to the library is scoped to the function.
//
// To keep the library interface stable and prevent name mangling, all library
// methods utilize C features only.

TtsService::TtsService(mojo::PendingReceiver<mojom::TtsService> receiver)
    : service_receiver_(this, std::move(receiver)),
      stream_receiver_(this),
      got_first_buffer_(false) {
  if (setpriority(PRIO_PROCESS, 0, -10 /* real time audio */) != 0) {
    PLOG(ERROR) << "Unable to request real time priority; performance will be "
                   "impacted.";
  }
  bool loaded = libchrometts_.Load(kLibchromettsPath);
  if (!loaded) {
    LOG(ERROR) << "Unable to load libchrometts.so.";
    exit(0);
  } else {
    libchrometts_.GoogleTtsSetLogger(HandleLibraryLogging);
  }
}

TtsService::~TtsService() = default;

void TtsService::BindTtsStream(
    mojo::PendingReceiver<mojom::TtsStream> receiver,
    mojo::PendingRemote<audio::mojom::StreamFactory> factory) {
  base::AutoLock al(state_lock_);
  stream_receiver_.Bind(std::move(receiver));

  // TODO(accessibility): The sample rate below can change based on the audio
  // data retrieved. Plumb this data through and re-create the output device if
  // it changes.
  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
      22050 /* sample rate */, libchrometts_.GoogleTtsGetFramesInAudioBuffer());

  output_device_ = std::make_unique<audio::OutputDevice>(
      std::move(factory), params, this, std::string());
}

void TtsService::InstallVoice(const std::string& voice_name,
                              const std::vector<uint8_t>& voice_bytes,
                              InstallVoiceCallback callback) {
  base::AutoLock al(state_lock_);

  // Create a directory to place extracted voice data.
  base::FilePath voice_data_path(kTempDataDirectory);
  voice_data_path = voice_data_path.Append(voice_name);
  if (base::DirectoryExists(voice_data_path)) {
    std::move(callback).Run(true);
    return;
  }

  if (!base::CreateDirectoryAndGetError(voice_data_path, nullptr)) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(libchrometts_.GoogleTtsInstallVoice(
      voice_data_path.value().c_str(), (char*)&voice_bytes[0],
      voice_bytes.size()));
}

void TtsService::SelectVoice(const std::string& voice_name,
                             SelectVoiceCallback callback) {
  base::AutoLock al(state_lock_);

  base::FilePath path_prefix =
      base::FilePath(kTempDataDirectory).Append(voice_name);
  base::FilePath pipeline_path = path_prefix.Append("pipeline");
  std::move(callback).Run(libchrometts_.GoogleTtsInit(
      pipeline_path.value().c_str(), path_prefix.value().c_str()));
}

void TtsService::Speak(const std::vector<uint8_t>& text_jspb,
                       SpeakCallback callback) {
  base::AutoLock al(state_lock_);

  tts_event_observer_.reset();
  auto pending_receiver = tts_event_observer_.BindNewPipeAndPassReceiver();
  std::move(callback).Run(std::move(pending_receiver));

  bool status = libchrometts_.GoogleTtsInitBuffered((char*)&text_jspb[0],
                                                    text_jspb.size());
  if (!status) {
    tts_event_observer_->OnError();
    return;
  }

  // For lower end devices, pre-fetching the first buffer on the main thread is
  // important. Not doing so can cause us to not respond quickly enough in the
  // audio rendering thread/callback below.
  size_t frames = 0;
  first_buf_.first.clear();
  first_buf_.first.resize(libchrometts_.GoogleTtsGetFramesInAudioBuffer());
  first_buf_.second =
      libchrometts_.GoogleTtsReadBuffered(&first_buf_.first[0], &frames);

  output_device_->Play();
}

void TtsService::Stop() {
  base::AutoLock al(state_lock_);
  StopLocked();
}

void TtsService::SetVolume(float volume) {
  base::AutoLock al(state_lock_);
  output_device_->SetVolume(volume);
}

int TtsService::Render(base::TimeDelta delay,
                       base::TimeTicks delay_timestamp,
                       int prior_frames_skipped,
                       media::AudioBus* dest) {
  // Careful to not block the render callback. Only try to acquire the lock
  // here, but early return if we are processing a series of other calls. This
  // can be extremely important if there's a long queue of pending Speak/Stop
  // pairs being processed on the main thread. This can occur if the tts api
  // receives lots of tts requests.
  if (!state_lock_.Try())
    return 0;

  size_t frames = 0;
  float* channel = dest->channel(0);
  int32_t status = -1;
  if (got_first_buffer_) {
    status = libchrometts_.GoogleTtsReadBuffered(channel, &frames);
  } else {
    status = first_buf_.second;
    float* buf = &first_buf_.first[0];
    frames = first_buf_.first.size();
    for (size_t i = 0; i < frames; i++)
      channel[i] = buf[i];
  }

  if (status <= 0) {
    // -1 means an error, 0 means done.
    if (status == -1)
      tts_event_observer_->OnError();

    dest->Zero();
    StopLocked();
    state_lock_.Release();
    return 0;
  }

  if (frames == 0) {
    state_lock_.Release();
    return 0;
  }

  if (!got_first_buffer_) {
    got_first_buffer_ = true;
    tts_event_observer_->OnStart();
  }

  // There's only really ever one timepoint since we play this buffer in one
  // chunk.
  int char_index = -1;
  if (libchrometts_.GoogleTtsGetTimepointsCount() > 0)
    char_index = libchrometts_.GoogleTtsGetTimepointsCharIndexAtIndex(0);

  if (char_index != -1)
    tts_event_observer_->OnTimepoint(char_index);

  state_lock_.Release();
  return frames;
}

void TtsService::OnRenderError() {}

void TtsService::StopLocked() {
  output_device_->Pause();
  libchrometts_.GoogleTtsFinalizeBuffered();
  if (tts_event_observer_ && got_first_buffer_)
    tts_event_observer_->OnEnd();
  got_first_buffer_ = false;
}

}  // namespace tts
}  // namespace chromeos
