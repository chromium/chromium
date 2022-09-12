// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_BUFFERING_FRAME_PROVIDER_H_
#define CHROMECAST_MEDIA_CMA_BASE_BUFFERING_FRAME_PROVIDER_H_

#include <stddef.h>

#include <list>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {
class DecoderBufferBase;

// BufferingFrameProvider -
// Fetch some data from another CodedFrameProvider up to a certain size limit.
class BufferingFrameProvider : public CodedFrameProvider {
 public:
  typedef base::RepeatingCallback<void(const scoped_refptr<DecoderBufferBase>&,
                                       bool)>
      FrameBufferedCB;

  // Creates a frame provider that buffers coded frames up to the
  // |max_buffer_size| limit (given as a number of bytes).
  // |max_frame_size| corresponds to an upper bound of the expected frame size.
  // Each time a frame is buffered, |frame_buffered_cb| is invoked with the
  // last frame buffered. The second parameter of the callback indicates
  // whether the maximum capacity has been reached, i.e. whether the next frame
  // size might overflow the buffer: |total_buffer_size_| + next_frame_size
  // might be greater than |max_buffer_size|.
  // Note: takes ownership of |coded_frame_provider|.
  BufferingFrameProvider(
      std::unique_ptr<CodedFrameProvider> coded_frame_provider,
      size_t max_buffer_size,
      size_t max_frame_size,
      const FrameBufferedCB& frame_buffered_cb);

  BufferingFrameProvider(const BufferingFrameProvider&) = delete;
  BufferingFrameProvider& operator=(const BufferingFrameProvider&) = delete;

  ~BufferingFrameProvider() override;

  // CodedFrameProvider implementation.
  void Read(ReadCB read_cb) override;
  void Flush(base::OnceClosure flush_cb) override;

 private:
  class BufferWithConfig {
   public:
    BufferWithConfig(
        const scoped_refptr<DecoderBufferBase>& buffer,
        const ::media::AudioDecoderConfig& audio_config,
        const ::media::VideoDecoderConfig& video_config);
    BufferWithConfig(const BufferWithConfig& other);
    ~BufferWithConfig();

    const scoped_refptr<DecoderBufferBase>& buffer() const { return buffer_; }
    const ::media::AudioDecoderConfig& audio_config() const {
      return audio_config_;
    }
    const ::media::VideoDecoderConfig& video_config() const {
      return video_config_;
    }

   private:
    scoped_refptr<DecoderBufferBase> buffer_;
    ::media::AudioDecoderConfig audio_config_;
    ::media::VideoDecoderConfig video_config_;
  };

  void OnNewBuffer(const scoped_refptr<DecoderBufferBase>& buffer,
                   const ::media::AudioDecoderConfig& audio_config,
                   const ::media::VideoDecoderConfig& video_config);
  void RequestBufferIfNeeded();
  void CompleteReadIfNeeded();

  base::ThreadChecker thread_checker_;

  // Frame provider the buffering frame provider fetches data from.
  std::unique_ptr<CodedFrameProvider> coded_frame_provider_;

  // Indicates whether there is a pending read request on
  // |coded_frame_provider_|.
  bool is_pending_request_;

  // Indicates whether the end of stream has been reached.
  bool is_eos_;

  std::list<BufferWithConfig> buffer_list_;

  // Size in bytes of audio/video buffers in |buffer_list_|.
  size_t total_buffer_size_;

  // Max amount of data to buffer.
  // i.e. this is the maximum size of buffers in |buffer_list_|.
  const size_t max_buffer_size_;

  // Maximum expected frame size.
  const size_t max_frame_size_;

  // Callback invoked each time there is a new frame buffered.
  FrameBufferedCB frame_buffered_cb_;

  // Pending read callback.
  ReadCB read_cb_;

  base::WeakPtr<BufferingFrameProvider> weak_this_;
  base::WeakPtrFactory<BufferingFrameProvider> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_BUFFERING_FRAME_PROVIDER_H_
