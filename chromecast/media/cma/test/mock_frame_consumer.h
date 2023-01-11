// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_TEST_MOCK_FRAME_CONSUMER_H_
#define CHROMECAST_MEDIA_CMA_TEST_MOCK_FRAME_CONSUMER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class CodedFrameProvider;
class DecoderBufferBase;
class FrameGeneratorForTest;

class MockFrameConsumer {
 public:
  explicit MockFrameConsumer(CodedFrameProvider* coded_frame_provider);

  MockFrameConsumer(const MockFrameConsumer&) = delete;
  MockFrameConsumer& operator=(const MockFrameConsumer&) = delete;

  ~MockFrameConsumer();

  void Configure(const std::vector<bool>& delayed_task_pattern,
                 bool last_read_aborted_by_flush,
                 std::unique_ptr<FrameGeneratorForTest> frame_generator);

  // Starts consuming frames. Invoke |done_cb| when all the expected frames
  // have been received.
  void Start(base::OnceClosure done_cb);

 private:
  void ReadFrame();
  void OnNewFrame(const scoped_refptr<DecoderBufferBase>& buffer,
                  const ::media::AudioDecoderConfig& audio_config,
                  const ::media::VideoDecoderConfig& video_config);

  void OnFlushCompleted();

  CodedFrameProvider* const coded_frame_provider_;

  base::OnceClosure done_cb_;

  // Parameterization of the frame consumer:
  // |delayed_task_pattern_| indicates the pattern for fetching frames,
  // i.e. after receiving a frame, either fetch a frame right away
  // or wait some time before fetching another frame.
  // |pattern_idx_| is the current index in the pattern.
  // |last_read_aborted_by_flush_| indicates whether the last buffer request
  // should be aborted by a Flush.
  std::vector<bool> delayed_task_pattern_;
  size_t pattern_idx_;
  bool last_read_aborted_by_flush_;

  // Expected results.
  std::unique_ptr<FrameGeneratorForTest> frame_generator_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_TEST_MOCK_FRAME_CONSUMER_H_
