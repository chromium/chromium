// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/demuxer_stream_adapter.h"

#include <list>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/base/demuxer_stream_for_test.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class DemuxerStreamAdapterTest : public testing::Test {
 public:
  DemuxerStreamAdapterTest();
  ~DemuxerStreamAdapterTest() override;

  void Initialize(::media::DemuxerStream* demuxer_stream);
  void Start();

 protected:
  void OnTestTimeout();
  void OnNewFrame(const scoped_refptr<DecoderBufferBase>& buffer,
                  const ::media::AudioDecoderConfig& audio_config,
                  const ::media::VideoDecoderConfig& video_config);
  void OnFlushCompleted();

  // Total number of frames to request.
  int total_frames_;

  // Number of demuxer read before issuing an early flush.
  int early_flush_idx_;
  bool use_post_task_for_flush_;

  // Number of expected read frames.
  int total_expected_frames_;

  // Number of frames actually read so far.
  int frame_received_count_;

  // List of expected frame indices with decoder config changes.
  std::list<int> config_idx_;

  std::unique_ptr<DemuxerStreamForTest> demuxer_stream_;

  std::unique_ptr<CodedFrameProvider> coded_frame_provider_;

  DISALLOW_COPY_AND_ASSIGN(DemuxerStreamAdapterTest);
};

DemuxerStreamAdapterTest::DemuxerStreamAdapterTest()
    : use_post_task_for_flush_(false) {
}

DemuxerStreamAdapterTest::~DemuxerStreamAdapterTest() {
}

void DemuxerStreamAdapterTest::Initialize(
    ::media::DemuxerStream* demuxer_stream) {
  coded_frame_provider_.reset(
      new DemuxerStreamAdapter(base::ThreadTaskRunnerHandle::Get(),
                               scoped_refptr<BalancedMediaTaskRunnerFactory>(),
                               demuxer_stream));
}

void DemuxerStreamAdapterTest::Start() {
  frame_received_count_ = 0;

  // TODO(damienv): currently, test assertions which fail do not trigger the
  // exit of the unit test, the message loop is still running. Find a different
  // way to exit the unit test.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DemuxerStreamAdapterTest::OnTestTimeout,
                     base::Unretained(this)),
      base::TimeDelta::FromSeconds(5));

  coded_frame_provider_->Read(base::Bind(&DemuxerStreamAdapterTest::OnNewFrame,
                                         base::Unretained(this)));
}

void DemuxerStreamAdapterTest::OnTestTimeout() {
  ADD_FAILURE() << "Test timed out";
  if (base::MessageLoopCurrent::Get())
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void DemuxerStreamAdapterTest::OnNewFrame(
    const scoped_refptr<DecoderBufferBase>& buffer,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (video_config.IsValidConfig()) {
    ASSERT_GT(config_idx_.size(), 0u);
    ASSERT_EQ(frame_received_count_, config_idx_.front());
    config_idx_.pop_front();
  }

  ASSERT_TRUE(buffer.get() != NULL);
  ASSERT_EQ(base::TimeDelta::FromMicroseconds(buffer->timestamp()),
            base::TimeDelta::FromMilliseconds(40 * frame_received_count_));
  frame_received_count_++;

  if (frame_received_count_ >= total_frames_) {
    coded_frame_provider_->Flush(base::Bind(
        &DemuxerStreamAdapterTest::OnFlushCompleted, base::Unretained(this)));
    return;
  }

  coded_frame_provider_->Read(base::Bind(&DemuxerStreamAdapterTest::OnNewFrame,
                                         base::Unretained(this)));

  ASSERT_LE(frame_received_count_, early_flush_idx_);
  if (frame_received_count_ == early_flush_idx_) {
    base::Closure flush_cb = base::Bind(
        &DemuxerStreamAdapterTest::OnFlushCompleted, base::Unretained(this));
    if (use_post_task_for_flush_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&CodedFrameProvider::Flush,
                         base::Unretained(coded_frame_provider_.get()),
                         flush_cb));
    } else {
      coded_frame_provider_->Flush(flush_cb);
    }
    return;
  }
}

void DemuxerStreamAdapterTest::OnFlushCompleted() {
  ASSERT_EQ(frame_received_count_, total_expected_frames_);
  ASSERT_FALSE(demuxer_stream_->IsReadPending());
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

TEST_F(DemuxerStreamAdapterTest, NoDelay) {
  total_frames_ = 10;
  early_flush_idx_ = total_frames_;  // No early flush.
  total_expected_frames_ = 10;
  config_idx_.push_back(0);
  config_idx_.push_back(5);

  int cycle_count = 1;
  int delayed_frame_count = 0;
  demuxer_stream_.reset(new DemuxerStreamForTest(
      -1, cycle_count, delayed_frame_count, config_idx_));

  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  Initialize(demuxer_stream_.get());
  message_loop->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DemuxerStreamAdapterTest::Start, base::Unretained(this)));
  base::RunLoop().Run();
}

TEST_F(DemuxerStreamAdapterTest, AllDelayed) {
  total_frames_ = 10;
  early_flush_idx_ = total_frames_;  // No early flush.
  total_expected_frames_ = 10;
  config_idx_.push_back(0);
  config_idx_.push_back(5);

  int cycle_count = 1;
  int delayed_frame_count = 1;
  demuxer_stream_.reset(new DemuxerStreamForTest(
      -1, cycle_count, delayed_frame_count, config_idx_));

  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  Initialize(demuxer_stream_.get());
  message_loop->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DemuxerStreamAdapterTest::Start, base::Unretained(this)));
  base::RunLoop().Run();
}

TEST_F(DemuxerStreamAdapterTest, AllDelayedEarlyFlush) {
  total_frames_ = 10;
  early_flush_idx_ = 5;
  use_post_task_for_flush_ = true;
  total_expected_frames_ = 5;
  config_idx_.push_back(0);
  config_idx_.push_back(3);

  int cycle_count = 1;
  int delayed_frame_count = 1;
  demuxer_stream_.reset(new DemuxerStreamForTest(
      -1, cycle_count, delayed_frame_count, config_idx_));

  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  Initialize(demuxer_stream_.get());
  message_loop->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DemuxerStreamAdapterTest::Start, base::Unretained(this)));
  base::RunLoop().Run();
}

}  // namespace media
}  // namespace chromecast
