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
    : service_receiver_(this, std::move(receiver)), stream_receiver_(this) {
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
  base::FilePath path_prefix =
      base::FilePath(kTempDataDirectory).Append(voice_name);
  base::FilePath pipeline_path = path_prefix.Append("pipeline");
  std::move(callback).Run(libchrometts_.GoogleTtsInit(
      pipeline_path.value().c_str(), path_prefix.value().c_str()));
}

void TtsService::Speak(const std::vector<uint8_t>& text_jspb,
                       SpeakCallback callback) {
  tts_event_observer_.reset();
  auto pending_receiver = tts_event_observer_.BindNewPipeAndPassReceiver();
  std::move(callback).Run(std::move(pending_receiver));

  bool status = libchrometts_.GoogleTtsInitBuffered((char*)&text_jspb[0],
                                                    text_jspb.size());
  if (!status) {
    tts_event_observer_->OnError();
    return;
  }

  output_device_->Play();

  is_playing_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&TtsService::ReadMoreFrames, base::Unretained(this),
                     true /* is_first_buffer */));
}

void TtsService::Stop() {
  base::AutoLock al(state_lock_);
  StopLocked();
}

void TtsService::SetVolume(float volume) {
  output_device_->SetVolume(volume);
}

int TtsService::Render(base::TimeDelta delay,
                       base::TimeTicks delay_timestamp,
                       int prior_frames_skipped,
                       media::AudioBus* dest) {
  size_t frames_in_buf = 0;
  int32_t status = -1;
  int char_index = -1;
  bool is_first_buffer = false;
  {
    base::AutoLock al(state_lock_);
    if (buffers_.empty())
      return 0;

    const AudioBuffer& buf = buffers_.front();

    status = buf.status;

    // Done, 0, or error, -1.
    if (status <= 0) {
      if (status == -1)
        tts_event_observer_->OnError();
      else
        tts_event_observer_->OnEnd();

      StopLocked();
      return 0;
    }

    char_index = buf.char_index;
    is_first_buffer = buf.is_first_buffer;
    const float* frames = &buf.frames[0];
    frames_in_buf = buf.frames.size();
    float* channel = dest->channel(0);
    for (size_t i = 0; i < frames_in_buf; i++)
      channel[i] = frames[i];
    buffers_.pop_front();
  }

  if (is_first_buffer)
    tts_event_observer_->OnStart();

  if (frames_in_buf == 0)
    return 0;

  if (char_index != -1)
    tts_event_observer_->OnTimepoint(char_index);

  return frames_in_buf;
}

void TtsService::OnRenderError() {}

void TtsService::StopLocked() {
  if (!is_playing_)
    return;

  output_device_->Pause();
  buffers_.clear();
  libchrometts_.GoogleTtsFinalizeBuffered();
  is_playing_ = false;
}

void TtsService::ReadMoreFrames(bool is_first_buffer) {
  if (!is_playing_)
    return;

  AudioBuffer buf;
  buf.frames.resize(libchrometts_.GoogleTtsGetFramesInAudioBuffer());
  size_t frames_in_buf = 0;
  buf.status =
      libchrometts_.GoogleTtsReadBuffered(&buf.frames[0], &frames_in_buf);

  buf.frames.resize(frames_in_buf);

  buf.char_index = -1;
  if (libchrometts_.GoogleTtsGetTimepointsCount() > 0)
    buf.char_index = libchrometts_.GoogleTtsGetTimepointsCharIndexAtIndex(0);

  buf.is_first_buffer = is_first_buffer;

  {
    base::AutoLock al(state_lock_);
    buffers_.emplace_back(std::move(buf));
  }

  if (buf.status <= 0)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&TtsService::ReadMoreFrames, base::Unretained(this),
                     false /* is_first_buffer */));
}

TtsService::AudioBuffer::AudioBuffer() = default;

TtsService::AudioBuffer::~AudioBuffer() = default;

TtsService::AudioBuffer::AudioBuffer(TtsService::AudioBuffer&& other) {
  frames.swap(other.frames);
  status = other.status;
  char_index = other.char_index;
  is_first_buffer = other.is_first_buffer;
}

}  // namespace tts
}  // namespace chromeos
