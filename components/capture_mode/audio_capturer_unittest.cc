// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/capture_mode/audio_capturer.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/task/bind_post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace capture_mode {
namespace {

// Provides a real shared-memory + socket data pipe so that the underlying
// AudioInputDevice creates a real AudioDeviceThread which dispatches into
// AudioCapturer::Capture().
class RealPipeAudioStreamFactory : public audio::FakeStreamFactory {
 public:
  void set_on_created_callback(base::OnceClosure on_created) {
    on_created_ = std::move(on_created);
  }

  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      uint32_t shared_memory_count,
      bool enable_agc,
      media::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback created_callback) override {
    stream_receiver_ = std::move(stream_receiver);
    client_.Bind(std::move(client));

    segment_length_ = media::ComputeAudioInputBufferSize(params, 1u);
    const uint32_t buffer_size = segment_length_ * shared_memory_count;
    auto shared_memory = base::UnsafeSharedMemoryRegion::Create(buffer_size);
    CHECK(shared_memory.IsValid());

    shared_memory_mapping_ = shared_memory.Map();
    CHECK(shared_memory_mapping_.IsValid());
    std::ranges::fill(shared_memory_mapping_.GetMemoryAsSpan<uint8_t>(), 0);

    base::CancelableSyncSocket foreign_socket;
    CHECK(base::CancelableSyncSocket::CreatePair(&socket_, &foreign_socket));

    std::move(created_callback)
        .Run(media::mojom::ReadWriteAudioDataPipe::New(
                 std::move(shared_memory),
                 mojo::PlatformHandle(foreign_socket.Take())),
             /*initially_muted=*/false, base::UnguessableToken::Create());

    if (on_created_) {
      std::move(on_created_).Run();
    }
  }

  // Writes valid parameters into segment `id` and signals the audio thread to
  // process it. Returns whether the signal was delivered.
  bool DeliverPacket(uint32_t id) {
    auto segment = shared_memory_mapping_.GetMemoryAsSpan<uint8_t>().subspan(
        id * segment_length_, segment_length_);
    auto* buffer = reinterpret_cast<media::AudioInputBuffer*>(segment.data());
    buffer->params.id = id;
    buffer->params.size =
        segment_length_ - sizeof(media::AudioInputBufferParameters);
    buffer->params.capture_time_us =
        (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
    return socket_.Send(base::byte_span_from_ref(id)) == sizeof(id);
  }

 private:
  base::OnceClosure on_created_;
  mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver_;
  mojo::Remote<media::mojom::AudioInputStreamClient> client_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  uint32_t segment_length_ = 0;
  base::CancelableSyncSocket socket_;
};

}  // namespace

// Destroying an AudioCapturer while the realtime audio thread is dispatching
// into Capture() must be safe even when the owner has not explicitly called
// Stop(). The destructor is responsible for stopping the underlying capturer
// source so that the audio thread is joined before the members it touches go
// away.
TEST(AudioCapturerDestructionTest, DestroyWithoutStopJoinsAudioThread) {
  base::test::TaskEnvironment task_environment;
  RealPipeAudioStreamFactory stream_factory;

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(),
      /*sample_rate=*/16000, /*frames_per_buffer=*/160);

  base::RunLoop init_run_loop;
  stream_factory.set_on_created_callback(init_run_loop.QuitClosure());

  base::RunLoop run_loop;
  auto capturer = std::make_unique<AudioCapturer>(
      "test-device", stream_factory.MakeRemote(), params,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          [](base::RepeatingClosure quit_closure,
             std::unique_ptr<media::AudioBus> audio_bus,
             base::TimeTicks audio_capture_time) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure())));
  capturer->Start();

  // Wait for mojo to deliver the creation call.
  init_run_loop.Run();

  // Verify the realtime audio thread is dispatching into the capturer.
  ASSERT_TRUE(stream_factory.DeliverPacket(0));
  run_loop.Run();

  // Destroy without an explicit Stop(); this must join the audio thread.
  capturer.reset();

  // Any subsequent packet must not reach the destroyed capturer. Either the
  // socket is already shut down (Send() fails), or the joined thread is no
  // longer reading from it.
  stream_factory.DeliverPacket(1);

  // Give a hypothetical un-joined audio thread time to act on the packet so
  // that sanitizers can observe any lifetime issue.
  base::PlatformThread::Sleep(base::Milliseconds(500));
}

}  // namespace capture_mode
