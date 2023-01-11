// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_CODED_FRAME_PROVIDER_H_
#define CHROMECAST_MEDIA_CMA_BASE_CODED_FRAME_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class DecoderBufferBase;

class CodedFrameProvider {
 public:
  typedef base::OnceCallback<void(const scoped_refptr<DecoderBufferBase>&,
                                  const ::media::AudioDecoderConfig&,
                                  const ::media::VideoDecoderConfig&)>
      ReadCB;

  CodedFrameProvider();

  CodedFrameProvider(const CodedFrameProvider&) = delete;
  CodedFrameProvider& operator=(const CodedFrameProvider&) = delete;

  virtual ~CodedFrameProvider();

  // Request a coded frame which is provided asynchronously through callback
  // |read_cb|.
  // If the frame is associated with a new video/audio configuration,
  // these configurations are returned as part of the |read_cb| callback.
  // Invoking the |read_cb| callback with invalid audio/video configurations
  // means the configurations have not changed.
  virtual void Read(ReadCB read_cb) = 0;

  // Flush the coded frames held by the frame provider.
  // Invoke callback |flush_cb| when completed.
  // Note: any pending read is cancelled, meaning that any pending |read_cb|
  // callback will not be invoked.
  // TODO(alokp): Delete this function once CmaRenderer is deprecated.
  virtual void Flush(base::OnceClosure flush_cb) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_CODED_FRAME_PROVIDER_H_
