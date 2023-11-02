// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_TEST_FRAME_GENERATOR_FOR_TEST_H_
#define CHROMECAST_MEDIA_CMA_TEST_FRAME_GENERATOR_FOR_TEST_H_

#include <stddef.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace chromecast {
namespace media {
class DecoderBufferBase;

class FrameGeneratorForTest {
 public:
  // Parameters used to generate frames.
  struct FrameSpec {
    FrameSpec();
    ~FrameSpec();

    // Indicates whether the frame comes with a new decoder configuration.
    bool has_config;

    bool is_eos;
    base::TimeDelta timestamp;
    bool has_decrypt_config;
    size_t size;
  };

  explicit FrameGeneratorForTest(const std::vector<FrameSpec> frame_specs);

  FrameGeneratorForTest(const FrameGeneratorForTest&) = delete;
  FrameGeneratorForTest& operator=(const FrameGeneratorForTest&) = delete;

  ~FrameGeneratorForTest();

  // Indicates whether the next frame should come with a new decoder config.
  bool HasDecoderConfig() const;

  // Generates a frame.
  // Returns NULL is there is no frame left to generate.
  scoped_refptr<DecoderBufferBase> Generate();

  // Number of frames not generated yet.
  size_t RemainingFrameCount() const;

 private:
  std::vector<FrameSpec> frame_specs_;
  size_t frame_idx_;

  // Total size of A/V buffers generated so far.
  size_t total_buffer_size_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_TEST_FRAME_GENERATOR_FOR_TEST_H_
