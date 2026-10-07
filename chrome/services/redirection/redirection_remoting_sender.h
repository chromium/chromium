// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_REDIRECTION_REDIRECTION_REMOTING_SENDER_H_
#define CHROME_SERVICES_REDIRECTION_REDIRECTION_REMOTING_SENDER_H_

#include <wrl/client.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/microsoft_multimedia_redirection/src/api/Mmr_h.h"

namespace media {
class DecoderBuffer;
}  // namespace media

namespace media::cast {
class DecoderBufferReader;
}  // namespace media::cast

namespace redirection {

// Sender for a single remoting data stream. The client calls SetStream() to
// instruct the sender to read media bitstream data from a Mojo data pipe and
// transmit it using an IMMRStream.
class RedirectionRemotingSender final
    : public media::mojom::RemotingDataStreamSender {
 public:
  RedirectionRemotingSender(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          stream_sender,
      base::OnceClosure error_callback);
  RedirectionRemotingSender(const RedirectionRemotingSender&) = delete;
  RedirectionRemotingSender& operator=(const RedirectionRemotingSender&) =
      delete;
  ~RedirectionRemotingSender() override;

  // Sets the sink-side stream this sender writes to, and starts reading from
  // the data pipe.
  void SetStream(IMMRStream* stream);

 private:
  // media::mojom::RemotingDataStreamSender implementation.
  void SendFrame(media::mojom::DecoderBufferPtr buffer,
                 SendFrameCallback callback) override;
  void CancelInFlightData() override;

  // Called by `decoder_buffer_reader_` once a frame is fully read. Forwards
  // uncanceled frames to `data_stream_` and requests another.
  void OnFrameRead(scoped_refptr<media::DecoderBuffer> buffer);

  void OnRemotingDataStreamError();

  Microsoft::WRL::ComPtr<IMMRStream> data_stream_;

  std::unique_ptr<media::cast::DecoderBufferReader> decoder_buffer_reader_;
  SendFrameCallback read_complete_cb_;
  bool discard_in_flight_frame_ = false;
  mojo::Receiver<media::mojom::RemotingDataStreamSender> stream_sender_;
  base::OnceClosure error_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RedirectionRemotingSender> weak_factory_{this};
};

}  // namespace redirection

#endif  // CHROME_SERVICES_REDIRECTION_REDIRECTION_REMOTING_SENDER_H_
