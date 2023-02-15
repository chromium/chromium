// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio/audio_output_provider_impl.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromeos/ash/services/libassistant/test_support/fake_platform_delegate.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {
namespace {
using assistant::FakePlatformDelegate;
using assistant::mojom::AssistantAudioDecoderFactory;
using ::assistant_client::OutputStreamMetadata;
using ::base::test::ScopedFeatureList;
using ::base::test::SingleThreadTaskEnvironment;

constexpr char kFakeDeviceId[] = "device_id";
}  // namespace

class FakeAudioOutputDelegate : public assistant_client::AudioOutput::Delegate {
 public:
  FakeAudioOutputDelegate() : thread_("assistant") { thread_.Start(); }

  FakeAudioOutputDelegate(const FakeAudioOutputDelegate&) = delete;
  FakeAudioOutputDelegate& operator=(const FakeAudioOutputDelegate&) = delete;

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

    // AudioDeviceOwner::ScheduleFillLocked() will be called repeatedlly until
    // the |num_bytes| is 0. Only call QuitClosure() at the last call to unblock
    // in the test.
    // Otherwise, the |run_loop_| may not block because the QuitClosure() is
    // called before Run(), right after it is created in Reset(), which will
    // cause timing issue in the test.
    if (num_bytes == 0) {
      quit_closure_.Run();
    }
  }

  bool end_of_stream() { return end_of_stream_; }

  void set_num_of_bytes_to_fill(int bytes) { num_bytes_to_fill_ = bytes; }

  void Reset() {
    run_loop_ = std::make_unique<base::RunLoop>();
    quit_closure_ =
        base::BindPostTaskToCurrentDefault(run_loop_->QuitClosure());
  }

  void Wait() { run_loop_->Run(); }

 private:
  base::Thread thread_;
  base::RepeatingClosure quit_closure_;
  std::unique_ptr<base::RunLoop> run_loop_;
  int num_bytes_to_fill_ = 0;
  bool end_of_stream_ = false;
};

class FakeAudioOutputDelegateMojom : public mojom::AudioOutputDelegate {
 public:
  FakeAudioOutputDelegateMojom() = default;
  FakeAudioOutputDelegateMojom(const FakeAudioOutputDelegateMojom&) = delete;
  FakeAudioOutputDelegateMojom& operator=(const FakeAudioOutputDelegateMojom&) =
      delete;
  ~FakeAudioOutputDelegateMojom() override = default;

  // libassistant::mojom::AudioOutputDelegate implementation:
  void RequestAudioFocus(mojom::AudioOutputStreamType stream_type) override {}
  void AbandonAudioFocusIfNeeded() override {}
  void AddMediaSessionObserver(
      mojo::PendingRemote<::media_session::mojom::MediaSessionObserver>
          observer) override {}
};

class FakeAssistantAudioDecoderFactory : public AssistantAudioDecoderFactory {
 public:
  FakeAssistantAudioDecoderFactory() = default;

  void CreateAssistantAudioDecoder(
      ::mojo::PendingReceiver<::ash::assistant::mojom::AssistantAudioDecoder>
          audio_decoder,
      ::mojo::PendingRemote<
          ::ash::assistant::mojom::AssistantAudioDecoderClient> client,
      ::mojo::PendingRemote<::ash::assistant::mojom::AssistantMediaDataSource>
          data_source) override {}
};

class AssistantAudioDeviceOwnerTest : public testing::Test {
 public:
  AssistantAudioDeviceOwnerTest()
      : task_env_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  AssistantAudioDeviceOwnerTest(const AssistantAudioDeviceOwnerTest&) = delete;
  AssistantAudioDeviceOwnerTest& operator=(
      const AssistantAudioDeviceOwnerTest&) = delete;

  ~AssistantAudioDeviceOwnerTest() override { task_env_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_env_;
};

TEST(AudioOutputProviderImplTest, StartDecoderServiceWithBindCall) {
  ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kStartAssistantAudioDecoderOnDemand);

  SingleThreadTaskEnvironment task_environment;

  auto provider = std::make_unique<AudioOutputProviderImpl>(kFakeDeviceId);

  FakePlatformDelegate platform_delegate;
  mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate;
  { auto unused = audio_output_delegate.InitWithNewPipeAndPassReceiver(); }
  provider->Bind(std::move(audio_output_delegate), &platform_delegate);

  provider->BindAudioDecoderFactory();

  mojo::PendingReceiver<AssistantAudioDecoderFactory>
      audio_decoder_factory_pending_receiver =
          platform_delegate.audio_decoder_factory_receiver();
  FakeAssistantAudioDecoderFactory fake_assistant_audio_decoder_factory;
  mojo::Receiver<AssistantAudioDecoderFactory>
      assistant_audio_decoder_factory_receiver(
          &fake_assistant_audio_decoder_factory,
          std::move(audio_decoder_factory_pending_receiver));
  // If the flag is off, we expect that AudioDecoderFactory will be bound after
  // BindAudioDecoderFactory call.
  EXPECT_TRUE(assistant_audio_decoder_factory_receiver.is_bound());

  bool disconnected = false;
  assistant_audio_decoder_factory_receiver.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { disconnected = true; }));

  provider->UnBindAudioDecoderFactory();
  task_environment.RunUntilIdle();

  // Confirm that it's disconnected after UnBindAudioDecoderFactory call.
  EXPECT_TRUE(disconnected);
}

TEST(AudioOutputProviderImplTest, StartDecoderServiceOnDemand) {
  ASSERT_TRUE(features::IsStartAssistantAudioDecoderOnDemandEnabled());
  SingleThreadTaskEnvironment task_environment;

  auto provider = std::make_unique<AudioOutputProviderImpl>(kFakeDeviceId);

  FakePlatformDelegate platform_delegate;
  mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate;
  { auto unused = audio_output_delegate.InitWithNewPipeAndPassReceiver(); }
  provider->Bind(std::move(audio_output_delegate), &platform_delegate);

  provider->BindAudioDecoderFactory();
  // If the flag is on, AudioDecoderFactory should not be bound with
  // BindAudioDecoderFactory, i.e. It should not be valid.
  EXPECT_FALSE(platform_delegate.audio_decoder_factory_receiver().is_valid());

  // Set encoding format to MP3 as we use AudioDecoder only if it's in encoded
  // format.
  OutputStreamMetadata metadata = {
      .buffer_stream_format = {
          .encoding = assistant_client::OutputStreamEncoding::STREAM_MP3,
      }};

  std::unique_ptr<assistant_client::AudioOutput> first_output(
      provider->CreateAudioOutput(metadata));
  FakeAudioOutputDelegate first_fake_audio_output_delegate;
  first_output->Start(&first_fake_audio_output_delegate);
  task_environment.RunUntilIdle();

  FakeAssistantAudioDecoderFactory fake_assistant_audio_decoder_factory;
  auto receiver =
      std::make_unique<mojo::Receiver<AssistantAudioDecoderFactory>>(
          &fake_assistant_audio_decoder_factory,
          platform_delegate.audio_decoder_factory_receiver());

  // Confirm that AudioDecoderFactory is now bound after Start call.
  EXPECT_TRUE(receiver->is_bound());

  // Create/Start another output as |second_output|.
  std::unique_ptr<assistant_client::AudioOutput> second_output(
      provider->CreateAudioOutput(metadata));
  FakeAudioOutputDelegate second_fake_audio_output_delegate;
  second_output->Start(&second_fake_audio_output_delegate);
  task_environment.RunUntilIdle();

  bool disconnected = false;
  receiver->set_disconnect_handler(
      base::BindLambdaForTesting([&]() { disconnected = true; }));

  // Delete the first output and confirm that the connection is not disconnected
  // as we still have the second output.
  first_output.reset();
  task_environment.RunUntilIdle();
  EXPECT_TRUE(receiver->is_bound());
  EXPECT_FALSE(disconnected);

  second_output.reset();
  task_environment.RunUntilIdle();
  // Confirm that AudioDecoderFactory is disconnected once all outputs got
  // deleted.
  EXPECT_TRUE(disconnected);

  // Create another one and confirm that it still works correctly.
  std::unique_ptr<assistant_client::AudioOutput> third_output(
      provider->CreateAudioOutput(metadata));
  FakeAudioOutputDelegate third_fake_audio_output_delegate;
  third_output->Start(&third_fake_audio_output_delegate);
  task_environment.RunUntilIdle();

  receiver = std::make_unique<mojo::Receiver<AssistantAudioDecoderFactory>>(
      &fake_assistant_audio_decoder_factory,
      platform_delegate.audio_decoder_factory_receiver());
  EXPECT_TRUE(receiver->is_bound());
  disconnected = false;
  receiver->set_disconnect_handler(
      base::BindLambdaForTesting([&]() { disconnected = true; }));

  third_output.reset();
  task_environment.RunUntilIdle();
  EXPECT_TRUE(disconnected);

  provider->UnBindAudioDecoderFactory();
}

// We do not use AssistantAudioDecoder if audio format is in raw format.
TEST(AudioOutputProviderImplTest, DoNotStartAudioServiceForRawFormat) {
  ScopedFeatureList scoped_feature_list(
      features::kStartAssistantAudioDecoderOnDemand);
  SingleThreadTaskEnvironment task_environment;
  FakeAssistantAudioDecoderFactory fake_assistant_audio_decoder_factory;

  auto provider = std::make_unique<AudioOutputProviderImpl>(kFakeDeviceId);

  FakePlatformDelegate platform_delegate;
  mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate;
  { auto unused = audio_output_delegate.InitWithNewPipeAndPassReceiver(); }
  provider->Bind(std::move(audio_output_delegate), &platform_delegate);

  provider->BindAudioDecoderFactory();
  EXPECT_FALSE(platform_delegate.audio_decoder_factory_receiver().is_valid());

  OutputStreamMetadata metadata = {
      .buffer_stream_format = {
          .encoding = assistant_client::OutputStreamEncoding::STREAM_PCM_S16,
          .pcm_sample_rate = 44800,
          .pcm_num_channels = 2,
      }};

  std::unique_ptr<assistant_client::AudioOutput> output(
      provider->CreateAudioOutput(metadata));
  FakeAudioOutputDelegate fake_audio_output_delegate;
  output->Start(&fake_audio_output_delegate);
  fake_audio_output_delegate.Reset();
  fake_audio_output_delegate.Wait();
  task_environment.RunUntilIdle();

  // Confirm that AudioDecoderFactory is not bound even after Start call if it's
  // in raw format.
  EXPECT_FALSE(platform_delegate.audio_decoder_factory_receiver().is_valid());

  output.reset();
  task_environment.RunUntilIdle();

  provider->UnBindAudioDecoderFactory();
  task_environment.RunUntilIdle();
}

// TODO(b/234874756): Move AssistantAudioDeviceOwner test under
// audio_device_owner_unittest.cc
TEST_F(AssistantAudioDeviceOwnerTest, BufferFilling) {
  FakeAudioOutputDelegateMojom audio_output_delegate_mojom;
  FakeAudioOutputDelegate audio_output_delegate;
  auto audio_bus = media::AudioBus::Create(2, 4480);
  assistant_client::OutputStreamFormat format{
      assistant_client::OutputStreamEncoding::STREAM_PCM_S16,
      44800,  // pcm_sample rate.
      2       // pcm_num_channels,
  };

  audio_output_delegate.set_num_of_bytes_to_fill(200);
  audio_output_delegate.Reset();

  auto owner = std::make_unique<AudioDeviceOwner>(kFakeDeviceId);
  // Upon start, it will start to fill the buffer. The fill should stop after
  // Wait().
  owner->Start(&audio_output_delegate_mojom, &audio_output_delegate,
               mojo::NullRemote(), format);
  audio_output_delegate.Wait();

  audio_output_delegate.Reset();
  audio_bus->Zero();
  // On first render, it will push the data to |audio_bus|.
  owner->Render(base::Microseconds(0), base::TimeTicks::Now(), {},
                audio_bus.get());
  audio_output_delegate.Wait();
  EXPECT_FALSE(audio_bus->AreFramesZero());
  EXPECT_FALSE(audio_output_delegate.end_of_stream());

  // The subsequent Render call will detect no data available and notify
  // delegate for OnEndOfStream().
  owner->Render(base::Microseconds(0), base::TimeTicks::Now(), {},
                audio_bus.get());
  EXPECT_TRUE(audio_output_delegate.end_of_stream());
}

}  // namespace ash::libassistant
