// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_TTS_TEST_UTILS_H_
#define CHROMEOS_SERVICES_TTS_TTS_TEST_UTILS_H_

#include "base/test/task_environment.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace tts {

// Mock implementation of the AudioStreamFactory mojo interface.
class MockAudioStreamFactory : public media::mojom::AudioStreamFactory {
 public:
  MockAudioStreamFactory();
  MockAudioStreamFactory(const MockAudioStreamFactory&) = delete;
  MockAudioStreamFactory& operator=(const MockAudioStreamFactory&) = delete;
  ~MockAudioStreamFactory() override;

  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      base::ReadOnlySharedMemoryRegion key_press_count_buffer,
      media::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback callback) override {}
  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) override {}
  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      base::OnceCallback<void(media::mojom::ReadWriteAudioDataPipePtr)>
          callback) override;
  void CreateSwitchableOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
          device_switch_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      base::OnceCallback<void(media::mojom::ReadWriteAudioDataPipePtr)>
          callback) override;
  void BindMuter(
      mojo::PendingAssociatedReceiver<media::mojom::LocalMuter> receiver,
      const base::UnguessableToken& group_id) override {}
  void CreateLoopbackStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      const base::UnguessableToken& group_id,
      base::OnceCallback<void(media::mojom::ReadOnlyAudioDataPipePtr)> callback)
      override {}

  mojo::PendingReceiver<media::mojom::AudioOutputStream> audio_output_stream_;
};

// Mock implementation of the TtsEventObserver mojo interface. Tests can use
// this to check how many times TTS event callbacks have been invoked.
class MockTtsEventObserver : public mojom::TtsEventObserver {
 public:
  MockTtsEventObserver();
  MockTtsEventObserver(const MockTtsEventObserver&) = delete;
  MockTtsEventObserver& operator=(const MockTtsEventObserver&) = delete;
  ~MockTtsEventObserver() override;

  // mojom::TtsEventObserver:
  void OnStart() override;
  void OnTimepoint(int32_t char_index) override;
  void OnEnd() override;
  void OnError() override {}

  int start_count = 0;
  std::vector<int> char_indices;
  int end_count = 0;
};

// Base class for TTS service tests. Constructs a MockAudioStreamFactory and all
// its dependencies, and provides a TaskEnvironment.
class TtsTestBase : public testing::Test {
 public:
  TtsTestBase();
  TtsTestBase(const TtsTestBase&) = delete;
  TtsTestBase& operator=(const TtsTestBase&) = delete;
  ~TtsTestBase() override;

  base::test::TaskEnvironment task_environment_;
  MockAudioStreamFactory mock_audio_stream_factory_;
  mojo::Receiver<media::mojom::AudioStreamFactory> audio_stream_factory_;
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_TTS_TEST_UTILS_H_
