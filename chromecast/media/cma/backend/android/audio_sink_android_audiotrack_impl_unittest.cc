// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/audio_sink_android_audiotrack_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/backend/android/audio_sink_android.h"
#include "chromecast/media/cma/backend/android/media_pipeline_backend_android.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/public/volume_control.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

constexpr int kChannels = 2;
constexpr int kSampleRate = 48000;
constexpr int kBytesPerFrame = kChannels * sizeof(float);
constexpr int kBytesPerSecond = kSampleRate * kBytesPerFrame;

constexpr size_t kFirstBufferBytes = 480 * 1024;
constexpr size_t kNextBufferBytes = 64 * 1024;
constexpr int kMaxFollowupBuffers = 6;

scoped_refptr<DecoderBufferBase> MakePcmBuffer(size_t bytes, int64_t pts_us) {
  auto buffer = base::MakeRefCounted<::media::DecoderBuffer>(bytes);
  std::ranges::fill(buffer->writable_span(), 0u);
  auto adapter = base::MakeRefCounted<DecoderBufferAdapter>(std::move(buffer));
  adapter->set_timestamp(base::Microseconds(pts_us));
  return adapter;
}

class StreamingDelegate : public AudioSinkAndroid::Delegate {
 public:
  void set_sink(AudioSinkAndroidAudioTrackImpl* sink) { sink_ = sink; }
  void set_max_followup_buffers(int max_followup_buffers) {
    max_followup_buffers_ = max_followup_buffers;
  }
  void set_done_closure(base::OnceClosure done_closure) {
    done_closure_ = std::move(done_closure);
  }
  int completions() const { return completions_; }
  int pushed() const { return pushed_; }
  int errors() const { return errors_; }

  void OnWritePcmCompletion(
      MediaPipelineBackendAndroid::BufferStatus status) override {
    EXPECT_EQ(status, MediaPipelineBackendAndroid::kBufferSuccess);
    ++completions_;
    if (completions_ > max_followup_buffers_) {
      if (done_closure_) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(done_closure_));
      }
      return;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&StreamingDelegate::PushNextBuffer,
                                  base::Unretained(this)));
  }

  void OnSinkError(SinkError error) override { ++errors_; }

 private:
  void PushNextBuffer() {
    ++pushed_;
    sink_->WritePcm(MakePcmBuffer(kNextBufferBytes, pushed_ * 100000));
  }

  AudioSinkAndroidAudioTrackImpl* sink_ = nullptr;
  int max_followup_buffers_ = kMaxFollowupBuffers;
  base::OnceClosure done_closure_;
  int completions_ = 0;
  int pushed_ = 0;
  int errors_ = 0;
};

TEST(AudioSinkAndroidAudioTrackImplTest,
     PreventDelegateCallsInvalidatesInFlightCallbacks) {
  base::test::SingleThreadTaskEnvironment task_environment;

  StreamingDelegate delegate;
  delegate.set_max_followup_buffers(0);
  auto sink = std::make_unique<AudioSinkAndroidAudioTrackImpl>(
      &delegate, kChannels, kSampleRate, /*primary=*/true,
      /*use_hw_av_sync=*/false, "default", AudioContentType::kMedia);
  ASSERT_TRUE(
      sink->Initialize(/*audio_track_session_id=*/0, /*is_apk_audio=*/false));
  delegate.set_sink(sink.get());

  sink->SetPaused(false);
  sink->WritePcm(MakePcmBuffer(16 * 1024, 0));
  sink->PreventDelegateCalls();

  base::PlatformThread::Sleep(base::Milliseconds(250));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.completions(), 0);
  sink.reset();
}

TEST(AudioSinkAndroidAudioTrackImplTest,
     PlayPausePlayDoesNotDuplicateCompletion) {
  base::test::SingleThreadTaskEnvironment task_environment;

  StreamingDelegate delegate;
  auto sink = std::make_unique<AudioSinkAndroidAudioTrackImpl>(
      &delegate, kChannels, kSampleRate, /*primary=*/true,
      /*use_hw_av_sync=*/false, "default", AudioContentType::kMedia);
  ASSERT_TRUE(
      sink->Initialize(/*audio_track_session_id=*/0, /*is_apk_audio=*/false));
  delegate.set_sink(sink.get());

  const int64_t min_buffered_us =
      AudioSinkAndroidAudioTrackImpl::GetMinimumBufferedTime(kChannels,
                                                             kSampleRate) -
      120000;
  const int64_t track_buffer_bytes = 3 * min_buffered_us * kBytesPerFrame *
                                     kSampleRate /
                                     base::Time::kMicrosecondsPerSecond;
  const base::TimeDelta continue_write_time = base::Microseconds(
      (static_cast<int64_t>(kFirstBufferBytes) - track_buffer_bytes) *
      base::Time::kMicrosecondsPerSecond / kBytesPerSecond);

  sink->SetPaused(true);
  sink->WritePcm(MakePcmBuffer(kFirstBufferBytes, 0));
  base::PlatformThread::Sleep(base::Milliseconds(250));

  base::RunLoop run_loop;
  delegate.set_done_closure(run_loop.QuitClosure());

  sink->SetPaused(false);
  sink->SetPaused(true);
  sink->SetPaused(false);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      continue_write_time + base::Seconds(10));
  run_loop.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.errors(), 0);
  EXPECT_EQ(delegate.pushed(), kMaxFollowupBuffers);
  EXPECT_EQ(delegate.completions(), delegate.pushed() + 1);

  sink->PreventDelegateCalls();
  sink.reset();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
