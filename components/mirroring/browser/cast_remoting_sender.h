// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_BROWSER_CAST_REMOTING_SENDER_H_
#define COMPONENTS_MIRRORING_BROWSER_CAST_REMOTING_SENDER_H_

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {
class MojoDataPipeReader;
}  // namespace media

namespace mirroring {

// The callback that is used to send frame events to renderer process for
// logging purpose.
using FrameEventCallback =
    base::RepeatingCallback<void(const std::vector<media::cast::FrameEvent>&)>;

// RTP sender for a single Cast Remoting RTP stream. The client calls Send() to
// instruct the sender to read from a Mojo data pipe and transmit the data using
// a CastTransport. This entire class executes on the IO BrowserThread.
//
// This class is instantiated and owned by CastTransportHostFilter in response
// to IPC messages from an extension process to create RTP streams for the media
// remoting use case. CastTransportHostFilter is also responsible for destroying
// the instance in response to later IPCs.
//
// The Media Router provider extension controls the entire set-up process:
// First, it uses the cast.streaming APIs to create remoting API streams (which
// instantiates one or more CastRemotingSenders). Then, it sends a message via
// Media Router to a CastRemotingConnector to indicate the bitstream transport
// is ready. Finally, CastRemotingConnector calls FindAndBind() to look-up the
// CastRemotingSender instances and establish the Mojo bindings and data flows.
class CastRemotingSender : public media::mojom::RemotingDataStreamSender {
 public:
  // |transport| is expected to outlive this class.
  // |logging_flush_interval| must be greater than |base::TimeDelta()| if |cb|
  // is not null.
  CastRemotingSender(media::cast::CastTransport* transport,
                     const media::cast::CastTransportRtpConfig& config,
                     base::TimeDelta logging_flush_interval,
                     const FrameEventCallback& cb);
  ~CastRemotingSender() final;

  // Look-up a CastRemotingSender instance by its |rtp_stream_id| and then bind
  // to the given |request|. The client of the RemotingDataStreamSender will
  // then instruct this CastRemotingSender when to read from the data |pipe| and
  // send the data to the Cast Receiver. If the bind fails, or an error occurs
  // reading from the data pipe during later operation, the |error_callback| is
  // run.
  //
  // Threading note: This function is thread-safe, but its internal
  // implementation runs on the IO BrowserThread. If |error_callback| is run, it
  // will execute on the thread that called this function.
  static void FindAndBind(int32_t rtp_stream_id,
                          mojo::ScopedDataPipeConsumerHandle pipe,
                          media::mojom::RemotingDataStreamSenderRequest request,
                          base::OnceClosure error_callback);

 private:
  // Friend class for unit tests.
  friend class CastRemotingSenderTest;

  class RemotingRtcpClient;

  // media::mojom::RemotingDataStreamSender implementation. SendFrame() will
  // push callbacks onto the back of the input queue, and these may or may not
  // be processed at a later time. It depends on whether the data pipe has data
  // available or the CastTransport can accept more frames. CancelInFlightData()
  // is processed immediately, and will cause all pending operations to discard
  // data when they are processed later.
  void SendFrame(uint32_t frame_size) final;
  void CancelInFlightData() final;

  // Attempt to run next pending input task, popping the head of the input queue
  // as each task succeeds.
  void ProcessNextInputTask();

  // These are called via callbacks run from the input queue.
  // Consumes a frame of |size| from the associated Mojo data pipe.
  void ReadFrame(uint32_t size);
  // Sends out the frame to the receiver over network.
  void TrySendFrame();

  // Called when a frame is completely read/discarded from the data pipe.
  void OnFrameRead(bool success);

  // Called when an input task completes.
  void OnInputTaskComplete();

  // These are called to deliver RTCP feedback from the receiver.
  void OnReceivedCastMessage(const media::cast::RtcpCastMessage& cast_feedback);
  void OnReceivedRtt(base::TimeDelta round_trip_time);

  // Returns the number of frames that were sent to the CastTransport, but not
  // yet acknowledged. This is always a high watermark estimate, as frames may
  // have been acknowledged out-of-order. Also, this does not account for any
  // frames queued-up in input pipeline (i.e., in the Mojo data pipe, nor in
  // |next_frame_data_|).
  int NumberOfFramesInFlight() const;

  // Schedule and execute periodic checks for re-sending packets.  If no
  // acknowledgements have been received for "too long," CastRemotingSender will
  // speculatively re-send certain packets of an unacked frame to kick-start
  // re-transmission.  This is a last resort tactic to prevent the session from
  // getting stuck after a long outage.
  void ScheduleNextResendCheck();
  void ResendCheck();
  void ResendForKickstart();

  void RecordLatestFrameTimestamps(media::cast::FrameId frame_id,
                                   media::cast::RtpTimeTicks rtp_timestamp);
  media::cast::RtpTimeTicks GetRecordedRtpTimestamp(
      media::cast::FrameId frame_id) const;

  // If |frame_event_cb_| is not null, this calls |frame_event_cb_| to
  // periodically send the frame events to renderer process for logging.
  void SendFrameEvents();

  // Schedule and execute periodic sending of RTCP report to prevent keepalive
  // timeouts on receiver side during media pause.
  void ScheduleNextRtcpReport();
  void SendRtcpReport();

  void OnPipeError();

  // Unique identifier for the RTP stream and this CastRemotingSender.
  const int32_t rtp_stream_id_;

  // Sends encoded frames over the configured transport (e.g., UDP). It outlives
  // this class.
  media::cast::CastTransport* const transport_;

  const uint32_t ssrc_;

  const bool is_audio_;

  // The interval to send frame events to renderer process for logging. When
  // |frame_event_cb_| is not null, this must be greater than base::TimeDelta().
  const base::TimeDelta logging_flush_interval_;

  // The callback to send frame events to renderer process for logging.
  const FrameEventCallback frame_event_cb_;

  const base::TickClock* clock_;

  // Callback that is run to notify when a fatal error occurs.
  base::OnceClosure error_callback_;

  std::unique_ptr<media::MojoDataPipeReader> data_pipe_reader_;

  // Mojo binding for this instance. Implementation at the other end of the
  // message pipe uses the RemotingDataStreamSender interface to control when
  // this CastRemotingSender consumes from |pipe_|.
  mojo::Binding<RemotingDataStreamSender> binding_;

  // This is the maximum delay that the sender should get ack from receiver.
  // Otherwise, sender will call ResendForKickstart().
  base::TimeDelta max_ack_delay_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last frame sent.  This member is invalid until
  // |!last_send_time_.is_null()|.
  media::cast::FrameId last_sent_frame_id_;

  // The ID of the latest (not necessarily the last) frame that has been
  // acknowledged.  This member is invalid until |!last_send_time_.is_null()|.
  media::cast::FrameId latest_acked_frame_id_;

  // Counts the number of duplicate ACK that are being received.  When this
  // number reaches a threshold, the sender will take this as a sign that the
  // receiver hasn't yet received the first packet of the next frame.  In this
  // case, CastRemotingSender will trigger a re-send of the next frame.
  int duplicate_ack_counter_;

  // The most recently measured round trip time.
  base::TimeDelta current_round_trip_time_;

  // The next frame's payload data. Populated by call to OnFrameRead() when
  // reading succeeded.
  std::string next_frame_data_;

  // Ring buffer to keep track of recent frame RTP timestamps. This should
  // only be accessed through the Record/GetXX() methods. The index into this
  // ring buffer is the lower 8 bits of the FrameId.
  media::cast::RtpTimeTicks frame_rtp_timestamps_[256];

  // Queue of pending input operations. |input_queue_discards_remaining_|
  // indicates the number of operations where data should be discarded (due to
  // CancelInFlightData()).
  base::queue<base::RepeatingClosure> input_queue_;
  size_t input_queue_discards_remaining_;

  // Indicates whether the |data_pipe_reader_| is processing a reading request.
  bool is_reading_;

  // Set to true if the first frame has not yet been sent, or if a
  // CancelInFlightData() operation just completed. This causes TrySendFrame()
  // to mark the next frame as the start of a new sequence.
  bool flow_restart_pending_;

  // FrameEvents pending delivery via |frame_event_cb_|. No event is added if
  // |frame_event_cb_| is null.
  std::vector<media::cast::FrameEvent> recent_frame_events_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CastRemotingSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastRemotingSender);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_BROWSER_CAST_REMOTING_SENDER_H_
