// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_output_provider_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"
#include "chromeos/services/assistant/media_host.h"
#include "chromeos/services/assistant/media_session/assistant_media_session.h"
#include "chromeos/services/assistant/test_support/scoped_assistant_client.h"
#include "libassistant/shared/public/platform_audio_output.h"
#include "media/base/audio_bus.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

class FakeAudioOutputDelegate : public assistant_client::AudioOutput::Delegate {
 public:
  FakeAudioOutputDelegate() : thread_("assistant") { thread_.Start(); }

  ~FakeAudioOutputDelegate() override = default;

  // assistant_client::AudioOutput::Delegate overrides:
  void FillBuffer(void* buffer,
                  int buffer_size,
                  int64_t playback_timestamp,
                  assistant_client::Callback1<int> done_cb) override {
    // Fill some arbitrary stuff.
    memset(reinterpret_cast<uint8_t*>(buffer), '1', num_bytes_to_fill_);
    int filled_bytes = num_bytes_to_fill_;
    num_bytes_to_fill_ = 0;

    // We'll need to maintain the multi-threaded async semantics as the real
    // assistant. Otherwise, it'll cause re-entrance of locks.
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FakeAudioOutputDelegate::FillBufferDone,
                                  base::Unretained(this), std::move(done_cb),
                                  filled_bytes));
  }

  void OnEndOfStream() override { end_of_stream_ = true; }

  void OnError(assistant_client::AudioOutput::Error error) override {}

  void OnStopped() override {}

  void FillBufferDone(assistant_client::Callback1<int> cb, int num_bytes) {
    cb(num_bytes);
    quit_closure_.Run();
  }

  bool end_of_stream() { return end_of_stream_; }

  void set_num_of_bytes_to_fill(int bytes) { num_bytes_to_fill_ = bytes; }

  void Reset() {
    run_loop_.reset(new base::RunLoop());
    quit_closure_ = media::BindToCurrentLoop(run_loop_->QuitClosure());
  }

  void Wait() { run_loop_->Run(); }

 private:
  base::Thread thread_;
  base::RepeatingClosure quit_closure_;
  std::unique_ptr<base::RunLoop> run_loop_;
  int num_bytes_to_fill_ = 0;
  bool end_of_stream_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioOutputDelegate);
};

class AssistantAudioDeviceOwnerTest : public testing::Test {
 public:
  AssistantAudioDeviceOwnerTest()
      : task_env_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  ~AssistantAudioDeviceOwnerTest() override { task_env_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_env_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAudioDeviceOwnerTest);
};

TEST_F(AssistantAudioDeviceOwnerTest, BufferFilling) {
  FakeAudioOutputDelegate delegate;
  auto audio_bus = media::AudioBus::Create(2, 4480);
  assistant_client::OutputStreamFormat format{
      assistant_client::OutputStreamEncoding::STREAM_PCM_S16,
      44800,  // pcm_sample rate.
      2       // pcm_num_channels,
  };

  delegate.set_num_of_bytes_to_fill(200);
  delegate.Reset();
  ScopedAssistantClient client;
  MediaHost media_host(AssistantClient::Get(),
                       /*interaction_subscribers=*/nullptr);
  AssistantMediaSession media_session(&media_host);

  auto owner = std::make_unique<AudioDeviceOwner>(
      base::SequencedTaskRunnerHandle::Get(),
      base::SequencedTaskRunnerHandle::Get(), "test device");
  // Upon start, it will start to fill the buffer.
  owner->StartOnMainThread(&media_session, &delegate, mojo::NullRemote(),
                           format);
  delegate.Wait();

  delegate.Reset();
  audio_bus->Zero();
  // On first render, it will push the data to |audio_bus|. The fill should
  // stop by now.
  owner->Render(base::TimeDelta::FromMicroseconds(0), base::TimeTicks::Now(), 0,
                audio_bus.get());
  delegate.Wait();
  EXPECT_FALSE(audio_bus->AreFramesZero());
  EXPECT_FALSE(delegate.end_of_stream());

  // The subsequent Render call will detect no data available and notify
  // delegate for OnEndOfStream().
  owner->Render(base::TimeDelta::FromMicroseconds(0), base::TimeTicks::Now(), 0,
                audio_bus.get());
  EXPECT_TRUE(delegate.end_of_stream());
}

}  // namespace assistant
}  // namespace chromeos
