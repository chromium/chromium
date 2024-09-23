// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_REMOTING_SENDER_H_
#define COMPONENTS_MIRRORING_SERVICE_REMOTING_SENDER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/cast/sender/frame_sender.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace base {
class TickClock;
}  // namespace base

namespace media {
class DecoderBuffer;
}  // namespace media

namespace media::cast {
class DecoderBufferReader;
}  // namespace media::cast

namespace openscreen::cast {
class Sender;
}  // namespace openscreen::cast

namespace mirroring {

// RTP sender for a single Cast Remoting RTP stream. The client calls Send() to
// instruct the sender to read from a Mojo data pipe and transmit the data using
// a CastTransport.
class COMPONENT_EXPORT(MIRRORING_SERVICE) RemotingSender final
    : public media::mojom::RemotingDataStreamSender,
      public media::cast::FrameSender::Client {
 public:
  // New way of instantiating using an openscreen::cast::Sender. Since the
  // |Sender| instance is destroyed when renegotiation is complete, |this|
  // is also invalid and should be immediately torn down.
  RemotingSender(scoped_refptr<media::cast::CastEnvironment> cast_environment,
                 std::unique_ptr<openscreen::cast::Sender> sender,
                 const media::cast::FrameSenderConfig& config,
                 mojo::ScopedDataPipeConsumerHandle pipe,
                 mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
                     stream_sender,
                 base::OnceClosure error_callback);

  RemotingSender(const RemotingSender&) = delete;
  RemotingSender& operator=(const RemotingSender&) = delete;

  ~RemotingSender() override;

 private:
  // Ctor that takes a media::cast::FrameSender for unit tests.
  // TODO(issues.chromium.org/329781397): Remove unnecessary wrapper objects in
  // Chrome's implementation of the Cast sender.
  RemotingSender(scoped_refptr<media::cast::CastEnvironment> cast_environment,
                 std::unique_ptr<media::cast::FrameSender> sender,
                 const media::cast::FrameSenderConfig& config,
                 mojo::ScopedDataPipeConsumerHandle pipe,
                 mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
                     stream_sender,
                 base::OnceClosure error_callback);

  // Friend class for unit tests.
  friend class RemotingSenderTest;

  // Creates SenderEncodedFrames.
  class SenderEncodedFrameFactory;

  // media::mojom::RemotingDataStreamSender implementation.
  void SendFrame(media::mojom::DecoderBufferPtr buffer,
                 SendFrameCallback callback) override;
  void CancelInFlightData() override;

  // FrameSender::Client overrides.
  int GetNumberOfFramesInEncoder() const override;
  base::TimeDelta GetEncoderBacklogDuration() const override;
  void OnFrameCanceled(media::cast::FrameId frame_id) override;

  // Sends out the frame to the receiver over network if |frame_sender_| has
  // available space to handle it.
  void TrySendFrame();

  // Sets |next_frame_| once it has been read from the data pipe.
  void OnFrameRead(scoped_refptr<media::DecoderBuffer> buffer);

  // Called when |stream_sender_| is disconnected.
  void OnRemotingDataStreamError();

  // Clears the current frame and requests a new one be sent.
  void ClearCurrentFrame();

  // Returns true if OnRemotingDataStreamError was called.
  bool HadError() const {
    DCHECK_EQ(!decoder_buffer_reader_, !stream_sender_.is_bound());
    return !decoder_buffer_reader_;
  }

  SEQUENCE_CHECKER(sequence_checker_);

  // The backing frame sender implementation.
  std::unique_ptr<media::cast::FrameSender> frame_sender_;

  raw_ptr<const base::TickClock> clock_;

  // Callback that is run to notify when a fatal error occurs.
  base::OnceClosure error_callback_;

  // Reads media::DecoderBuffer instances and passes them to OnFrameRead().
  std::unique_ptr<media::cast::DecoderBufferReader> decoder_buffer_reader_;

  // Mojo receiver for this instance. Implementation at the other end of the
  // message pipe uses the RemotingDataStreamSender remote to control when
  // this RemotingSender consumes from |pipe_|.
  mojo::Receiver<media::mojom::RemotingDataStreamSender> stream_sender_;

  // Whether this is an audio sender (true) or a video sender (false).
  const bool is_audio_;

  // Responsible for creating encoded frames.
  std::unique_ptr<SenderEncodedFrameFactory> frame_factory_;

  // The next frame. Populated by call to OnFrameRead() when reading succeeded.
  scoped_refptr<media::DecoderBuffer> next_frame_;

  // To be called once a frame has been successfully read and this instance is
  // ready to process a new one.
  SendFrameCallback read_complete_cb_;

  // Set to true if the first frame has not yet been sent, or if a
  // CancelInFlightData() operation just completed. This causes TrySendFrame()
  // to mark the next frame as the start of a new sequence.
  bool flow_restart_pending_ = true;

  // Number of EnqueueFrame() calls that have failed since the last successful
  // call.
  int consecutive_enqueue_frame_failure_count_ = 0;

  // The next frame's ID. Before any frames are sent, this will be the ID of
  // the first frame.
  media::cast::FrameId next_frame_id_ = media::cast::FrameId::first();

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<RemotingSender> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_REMOTING_SENDER_H_
