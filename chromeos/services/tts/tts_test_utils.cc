// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_test_utils.h"

namespace chromeos {
namespace tts {

using CreateOutputStreamCallback =
    base::OnceCallback<void(media::mojom::ReadWriteAudioDataPipePtr)>;
using CreateLoopbackStreamCallback =
    base::OnceCallback<void(media::mojom::ReadOnlyAudioDataPipePtr)>;

MockAudioStreamFactory::MockAudioStreamFactory() = default;
MockAudioStreamFactory::~MockAudioStreamFactory() = default;

void MockAudioStreamFactory::CreateOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback callback) {
  audio_output_stream_ = std::move(stream);
  std::move(callback).Run(nullptr);
}

void MockAudioStreamFactory::CreateSwitchableOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
    mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
        device_switch_receiver,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback callback) {
  audio_output_stream_ = std::move(stream);
  std::move(callback).Run(nullptr);
}

MockTtsEventObserver::MockTtsEventObserver() = default;
MockTtsEventObserver::~MockTtsEventObserver() = default;
void MockTtsEventObserver::OnStart() {
  start_count++;
}

void MockTtsEventObserver::OnTimepoint(int32_t char_index) {
  char_indices.push_back(char_index);
}

void MockTtsEventObserver::OnEnd() {
  end_count++;
}

TtsTestBase::TtsTestBase()
    : audio_stream_factory_(&mock_audio_stream_factory_) {}
TtsTestBase::~TtsTestBase() = default;

}  // namespace tts
}  // namespace chromeos
