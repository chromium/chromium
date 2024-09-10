// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_FRAME_STREAM_CONSUMER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_FRAME_STREAM_CONSUMER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/cast_streaming/browser/common/decoder_buffer_factory.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_session.h"

namespace cast_streaming {

// Attaches to an Open Screen Receiver to receive buffers of encoded data and
// invokes |frame_received_cb_| with each buffer.
//
// Internally, this class writes buffers of encoded data directly to
// |data_pipe_| rather than using a helper class like MojoDecoderBufferWriter.
// This allows us to use |data_pipe_| as an end-to-end buffer to cap memory
// usage. Receiving new buffers is delayed until the pipe has free memory again.
// The Open Screen library takes care of discarding buffers that are too old and
// requesting new key frames as needed.
class StreamConsumer final : public openscreen::cast::Receiver::Consumer {
 public:
  using FrameReceivedCB =
      base::RepeatingCallback<void(media::mojom::DecoderBufferPtr)>;

  // |receiver| sends frames to this object. It must outlive this object.
  // |frame_received_cb| is called on every new frame, after a new frame has
  // been written to |data_pipe|. On error, |data_pipe| will be closed.
  // On every new frame, |on_new_frame| will be called.
  StreamConsumer(openscreen::cast::Receiver* receiver,
                 mojo::ScopedDataPipeProducerHandle data_pipe,
                 FrameReceivedCB frame_received_cb,
                 base::RepeatingClosure on_new_frame,
                 std::unique_ptr<DecoderBufferFactory> decoder_buffer_factory);
  ~StreamConsumer() override;

  StreamConsumer(const StreamConsumer&) = delete;
  StreamConsumer& operator=(const StreamConsumer&) = delete;

  // Informs the StreamConsumer that a new frame should be read asynchronously.
  // Eventually, the |frame_received_cb_| will be called with the data for this
  // frame. |no_frames_available_cb| will be called if no frames are immediately
  // available when this callback first tries to read them.
  void ReadFrame(base::OnceClosure no_frames_available_cb);

  // Cancels any ongoing read call, then skips reading of all future frames with
  // an id less than |frame_id|.
  void FlushUntil(uint32_t frame_id);

 private:
  // Wrapper around a data buffer used for storing the data of a DecoderBuffer
  // received from Openscreen.
  class BufferDataWrapper : public DecoderBufferFactory::FrameContents {
   public:
    ~BufferDataWrapper() override;

    // Returns up to |max_size| more bytes of the underlying array, invalidating
    // these bytes in the underlying buffer.
    base::span<uint8_t> Consume(uint32_t max_size);

    // DecoderBufferFactory::FrameContents overrides.
    base::span<uint8_t> Get() override;
    bool Reset(uint32_t new_size) override;
    void Clear() override;
    uint32_t Size() const override;

   private:
    // Maximum frame size that OnFramesReady() can accept.
    static constexpr size_t kMaxFrameSize = 512 * 1024;

    // Buffer backing the spans created by this class.
    uint8_t pending_buffer_[kMaxFrameSize];

    // Current offset for data in |pending_buffer_|.
    uint32_t pending_buffer_offset_ = 0;

    // Remaining bytes to write from |pending_buffer_|.
    uint32_t pending_buffer_remaining_bytes_ = 0;
  };

  // Closes |data_pipe_| and resets the Consumer in |receiver_|. No frames will
  // be received after this call.
  void CloseDataPipeOnError();

  // Callback when |data_pipe_| can be written to again after it was full.
  void OnPipeWritable(MojoResult result);

  // Processes a ready frame, if both one is ready and a read callback is
  // pending.
  void MaybeSendNextFrame();

  bool WriteBufferToDataPipe();

  // openscreen::cast::Receiver::Consumer implementation.
  void OnFramesReady(int next_frame_buffer_size) override;

  // This receiver should skip all frames with id less than this value. Set by a
  // call to FlushUntil() and 0 when no flush is ongoing.
  uint32_t skip_until_frame_id_ = 0;

  const raw_ptr<openscreen::cast::Receiver> receiver_;
  mojo::ScopedDataPipeProducerHandle data_pipe_;
  const FrameReceivedCB frame_received_cb_;

  BufferDataWrapper data_wrapper_;

  // Provides notifications about |data_pipe_| readiness.
  mojo::SimpleWatcher pipe_watcher_;

  bool is_read_pending_ = false;

  base::OnceClosure no_frames_available_cb_;

  // Closure called on every new frame.
  base::RepeatingClosure on_new_frame_;

  // Factory to use for creating DecoderBuffers.
  std::unique_ptr<DecoderBufferFactory> decoder_buffer_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_FRAME_STREAM_CONSUMER_H_
