// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/google_tts_stream.h"

#include <dlfcn.h>
#include <sys/resource.h>

#include "base/files/file_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/services/tts/constants.h"
#include "chromeos/services/tts/tts_service.h"

namespace chromeos {
namespace tts {

// Simple helper to bridge logging in the shared library to Chrome's logging.
void HandleLibraryLogging(int severity, const char* message) {
  switch (severity) {
    case logging::LOGGING_INFO:
      // Suppressed.
      break;
    case logging::LOGGING_WARNING:
      LOG(WARNING) << message;
      break;
    case logging::LOGGING_ERROR:
    case logging::LOGGING_FATAL:
      LOG(ERROR) << message;
      break;
    default:
      break;
  }
}

// GoogleTtsStream is mostly glue code that adapts the TtsStream interface into
// a form needed by libchrometts.so. As is convention with shared objects, the
// lifetime of all arguments passed to the library is scoped to the function.
//
// To keep the library interface stable and prevent name mangling, all library
// methods utilize C features only.

GoogleTtsStream::GoogleTtsStream(
    TtsService* owner,
    mojo::PendingReceiver<mojom::GoogleTtsStream> receiver,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> factory)
    : owner_(owner),
      stream_receiver_(this, std::move(receiver)),
      tts_player_(
          std::move(factory),
          media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 media::ChannelLayoutConfig::Mono(),
                                 kDefaultSampleRate,
                                 kDefaultBufferSize)) {
  bool loaded = libchrometts_.Load(kLibchromettsPath);
  if (!loaded) {
    LOG(ERROR) << "Unable to load libchrometts.so.";
    exit(0);
  } else {
    libchrometts_.GoogleTtsSetLogger(HandleLibraryLogging);
  }

  stream_receiver_.set_disconnect_handler(base::BindOnce(
      [](TtsService* owner, mojo::Receiver<mojom::GoogleTtsStream>* receiver) {
        // The remote which lives in component extension js has been
        // disconnected due to destruction or error.
        receiver->reset();
        owner->MaybeExit();
      },
      owner, &stream_receiver_));
}

GoogleTtsStream::~GoogleTtsStream() {
  if (!is_in_process_teardown_)
    libchrometts_.GoogleTtsShutdown();
}

bool GoogleTtsStream::IsBound() const {
  return stream_receiver_.is_bound();
}

void GoogleTtsStream::InstallVoice(const std::string& voice_name,
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
      voice_data_path.value().c_str(), &voice_bytes[0], voice_bytes.size()));
}

void GoogleTtsStream::SelectVoice(const std::string& voice_name,
                                  SelectVoiceCallback callback) {
  base::FilePath path_prefix =
      base::FilePath(kTempDataDirectory).Append(voice_name);
  base::FilePath pipeline_path = path_prefix.Append("pipeline.pb");
  std::move(callback).Run(libchrometts_.GoogleTtsInit(
      pipeline_path.value().c_str(), path_prefix.value().c_str()));
}

void GoogleTtsStream::Speak(const std::vector<uint8_t>& text_jspb,
                            const std::vector<uint8_t>& speaker_params_jspb,
                            SpeakCallback callback) {
  bool status = libchrometts_.GoogleTtsInitBuffered(
      &text_jspb[0], &speaker_params_jspb[0], text_jspb.size(),
      speaker_params_jspb.size());
  if (!status) {
    stream_receiver_.reset();
    owner_->MaybeExit();
    return;
  }

  tts_player_.Play(std::move(callback));
  is_buffering_ = true;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GoogleTtsStream::ReadMoreFrames,
                     weak_factory_.GetWeakPtr(), true /* is_first_buffer */));
}

void GoogleTtsStream::Stop() {
  tts_player_.Stop();
  is_buffering_ = false;
}

void GoogleTtsStream::SetVolume(float volume) {
  tts_player_.SetVolume(volume);
}

void GoogleTtsStream::Pause() {
  tts_player_.Pause();
}

void GoogleTtsStream::Resume() {
  tts_player_.Resume();
}

void GoogleTtsStream::ReadMoreFrames(bool is_first_buffer) {
  if (!is_buffering_)
    return;

  TtsPlayer::AudioBuffer buf;
  buf.frames.resize(libchrometts_.GoogleTtsGetFramesInAudioBuffer());
  size_t frames_in_buf = 0;
  const int status =
      libchrometts_.GoogleTtsReadBuffered(&buf.frames[0], &frames_in_buf);
  buf.status = status;

  buf.frames.resize(frames_in_buf);

  buf.char_index = -1;
  buf.is_first_buffer = is_first_buffer;

  tts_player_.AddAudioBuffer(std::move(buf));

  for (size_t timepoint_index = 0;
       timepoint_index < libchrometts_.GoogleTtsGetTimepointsCount();
       timepoint_index++) {
    tts_player_.AddExplicitTimepoint(
        libchrometts_.GoogleTtsGetTimepointsCharIndexAtIndex(timepoint_index),
        base::Seconds(libchrometts_.GoogleTtsGetTimepointsTimeInSecsAtIndex(
            timepoint_index)));
  }

  // Ensure we always clean up given status 0 (done) or -1 (error).
  if (status <= 0) {
    is_buffering_ = false;
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GoogleTtsStream::ReadMoreFrames,
                     weak_factory_.GetWeakPtr(), false /* is_first_buffer */));
}

}  // namespace tts
}  // namespace chromeos
