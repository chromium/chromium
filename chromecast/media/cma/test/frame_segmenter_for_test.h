// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_TEST_FRAME_SEGMENTER_FOR_TEST_H_
#define CHROMECAST_MEDIA_CMA_TEST_FRAME_SEGMENTER_FOR_TEST_H_

#include <stddef.h>
#include <stdint.h>

#include <list>

#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace base {
class FilePath;
}

namespace chromecast {
namespace media {
class DecoderBufferBase;

typedef std::list<scoped_refptr<DecoderBufferBase> > BufferList;

// Implement some basic frame segmenters good enough for unit tests.
BufferList Mp3SegmenterForTest(const uint8_t* data, size_t data_size);
BufferList H264SegmenterForTest(const uint8_t* data, size_t data_size);

struct DemuxResult {
  DemuxResult();
  DemuxResult(const DemuxResult& other);
  ~DemuxResult();

  ::media::AudioDecoderConfig audio_config;
  ::media::VideoDecoderConfig video_config;
  BufferList frames;
};

DemuxResult FFmpegDemuxForTest(const base::FilePath& filepath,
                               bool audio);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_TEST_FRAME_SEGMENTER_FOR_TEST_H_
