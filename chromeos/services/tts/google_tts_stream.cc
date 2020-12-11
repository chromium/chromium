// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/google_tts_stream.h"

#include <dlfcn.h>
#include <sys/resource.h>

#include "base/files/file_util.h"
#include "chromeos/services/tts/constants.h"
#include "chromeos/services/tts/tts_service.h"

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
    case logging::LOG_FATAL:
    case logging::LOG_ERROR:
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
    mojo::PendingReceiver<mojom::GoogleTtsStream> receiver)
    : owner_(owner), stream_receiver_(this, std::move(receiver)) {
  bool loaded = libchrometts_.Load(kLibchromettsPath);
  if (!loaded) {
    LOG(ERROR) << "Unable to load libchrometts.so.";
    exit(0);
  } else {
    libchrometts_.GoogleTtsSetLogger(HandleLibraryLogging);
  }

  stream_receiver_.set_disconnect_handler(base::BindOnce(
      [](TtsService* owner) {
        // The remote which lives in component extension js has been
        // disconnected due to destruction or error.
        owner->MaybeExit();
      },
      owner));
}

GoogleTtsStream::~GoogleTtsStream() = default;

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
                            const std::string& speaker_name,
                            SpeakCallback callback) {
  bool status = libchrometts_.GoogleTtsInitBuffered(
      &text_jspb[0], speaker_name.c_str(), text_jspb.size());
  if (!status) {
    stream_receiver_.reset();
    owner_->MaybeExit();
    return;
  }

  owner_->Play(std::move(callback));
  is_buffering_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GoogleTtsStream::ReadMoreFrames,
                     weak_factory_.GetWeakPtr(), true /* is_first_buffer */));
}

void GoogleTtsStream::Stop() {
  owner_->Stop();
  is_buffering_ = false;
  libchrometts_.GoogleTtsFinalizeBuffered();
}

void GoogleTtsStream::SetVolume(float volume) {
  owner_->SetVolume(volume);
}

void GoogleTtsStream::Pause() {
  owner_->Pause();
}

void GoogleTtsStream::Resume() {
  owner_->Resume();
}

void GoogleTtsStream::ReadMoreFrames(bool is_first_buffer) {
  if (!is_buffering_) {
    return;
  }

  TtsService::AudioBuffer buf;
  buf.frames.resize(libchrometts_.GoogleTtsGetFramesInAudioBuffer());
  size_t frames_in_buf = 0;
  const int status =
      libchrometts_.GoogleTtsReadBuffered(&buf.frames[0], &frames_in_buf);
  buf.status = status;

  buf.frames.resize(frames_in_buf);

  buf.char_index = -1;
  buf.is_first_buffer = is_first_buffer;

  owner_->AddAudioBuffer(std::move(buf));

  for (size_t timepoint_index = 0;
       timepoint_index < libchrometts_.GoogleTtsGetTimepointsCount();
       timepoint_index++) {
    owner_->AddExplicitTimepoint(
        libchrometts_.GoogleTtsGetTimepointsCharIndexAtIndex(timepoint_index),
        base::TimeDelta::FromSecondsD(
            libchrometts_.GoogleTtsGetTimepointsTimeInSecsAtIndex(
                timepoint_index)));
  }

  if (status <= 0)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GoogleTtsStream::ReadMoreFrames,
                     weak_factory_.GetWeakPtr(), false /* is_first_buffer */));
}

}  // namespace tts
}  // namespace chromeos
