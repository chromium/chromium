// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/demuxer_stream_adapter.h"
#include "chromecast/media/cma/base/demuxer_stream_for_test.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {
// Maximum pts diff between frames
const int kMaxPtsDiffMs = 2000;
}  // namespace

// Test for multiple streams
class MultiDemuxerStreamAdaptersTest : public testing::Test {
 public:
  MultiDemuxerStreamAdaptersTest();

  MultiDemuxerStreamAdaptersTest(const MultiDemuxerStreamAdaptersTest&) =
      delete;
  MultiDemuxerStreamAdaptersTest& operator=(
      const MultiDemuxerStreamAdaptersTest&) = delete;

  ~MultiDemuxerStreamAdaptersTest() override;

  void Start();
  void Run();

 protected:
  void OnTestTimeout();
  void OnNewFrame(CodedFrameProvider* frame_provider,
                  const scoped_refptr<DecoderBufferBase>& buffer,
                  const ::media::AudioDecoderConfig& audio_config,
                  const ::media::VideoDecoderConfig& video_config);

  // Number of expected read frames.
  int total_expected_frames_;

  // Number of frames actually read so far.
  int frame_received_count_;

  // List of expected frame indices with decoder config changes.
  std::list<int> config_idx_;

  std::vector<std::unique_ptr<DemuxerStreamForTest>> demuxer_streams_;

  std::vector<std::unique_ptr<CodedFrameProvider>> coded_frame_providers_;

 private:
  // exit if all of the streams end
  void OnEos();

  // Number of reading-streams
  int running_stream_count_;

  scoped_refptr<BalancedMediaTaskRunnerFactory> media_task_runner_factory_;

  base::OnceClosure quit_closure_;
};

MultiDemuxerStreamAdaptersTest::MultiDemuxerStreamAdaptersTest() {
}

MultiDemuxerStreamAdaptersTest::~MultiDemuxerStreamAdaptersTest() {
}
void MultiDemuxerStreamAdaptersTest::Run() {
  base::RunLoop loop;
  quit_closure_ = loop.QuitWhenIdleClosure();
  loop.Run();
}
void MultiDemuxerStreamAdaptersTest::Start() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MultiDemuxerStreamAdaptersTest::OnTestTimeout,
                     base::Unretained(this)),
      base::Seconds(5));

  media_task_runner_factory_ =
      new BalancedMediaTaskRunnerFactory(base::Milliseconds(kMaxPtsDiffMs));

  coded_frame_providers_.clear();
  frame_received_count_ = 0;

  for (const auto& stream : demuxer_streams_) {
    coded_frame_providers_.push_back(std::make_unique<DemuxerStreamAdapter>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        media_task_runner_factory_, stream.get()));
  }
  running_stream_count_ = coded_frame_providers_.size();

  // read each stream
  for (const auto& code_frame_provider : coded_frame_providers_) {
    auto read_cb =
        base::BindOnce(&MultiDemuxerStreamAdaptersTest::OnNewFrame,
                       base::Unretained(this), code_frame_provider.get());

    base::OnceClosure task = base::BindOnce(
        &CodedFrameProvider::Read, base::Unretained(code_frame_provider.get()),
        std::move(read_cb));

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(task));
  }
}

void MultiDemuxerStreamAdaptersTest::OnTestTimeout() {
  if (running_stream_count_ != 0) {
    ADD_FAILURE() << "Test timed out";
  }
}

void MultiDemuxerStreamAdaptersTest::OnNewFrame(
    CodedFrameProvider* frame_provider,
    const scoped_refptr<DecoderBufferBase>& buffer,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (buffer->end_of_stream()) {
    OnEos();
    return;
  }

  frame_received_count_++;
  auto read_cb = base::BindOnce(&MultiDemuxerStreamAdaptersTest::OnNewFrame,
                                base::Unretained(this), frame_provider);
  frame_provider->Read(std::move(read_cb));
}

void MultiDemuxerStreamAdaptersTest::OnEos() {
  running_stream_count_--;
  ASSERT_GE(running_stream_count_, 0);
  if (running_stream_count_ == 0) {
    ASSERT_EQ(frame_received_count_, total_expected_frames_);
    std::move(quit_closure_).Run();
  }
}

TEST_F(MultiDemuxerStreamAdaptersTest, EarlyEos) {
  // We have more than one streams here. One of them is much shorter than the
  // others. When the shortest stream reaches EOS, other streams should still
  // run as usually. BalancedTaskRunner should not be blocked.
  int frame_count_short = 2;
  int frame_count_long =
      frame_count_short +
      kMaxPtsDiffMs / DemuxerStreamForTest::kDemuxerStreamForTestFrameDuration +
      100;
  demuxer_streams_.push_back(std::make_unique<DemuxerStreamForTest>(
      frame_count_short, 2, 0, config_idx_));
  demuxer_streams_.push_back(std::make_unique<DemuxerStreamForTest>(
      frame_count_long, 10, 0, config_idx_));

  total_expected_frames_ = frame_count_short + frame_count_long;

  base::test::SingleThreadTaskEnvironment task_environment;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MultiDemuxerStreamAdaptersTest::Start,
                                base::Unretained(this)));
  Run();
}
}  // namespace media
}  // namespace chromecast
