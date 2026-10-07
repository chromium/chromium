// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/redirection/redirection_remoting_sender.h"

#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/win/mf_helpers.h"
#include "media/cast/openscreen/decoder_buffer_reader.h"
#include "mojo/public/cpp/bindings/message.h"

namespace redirection {

RedirectionRemotingSender::RedirectionRemotingSender(
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> stream_sender,
    base::OnceClosure error_callback)
    : decoder_buffer_reader_(std::make_unique<media::cast::DecoderBufferReader>(
          base::BindRepeating(&RedirectionRemotingSender::OnFrameRead,
                              base::Unretained(this)),
          std::move(pipe))),
      stream_sender_(this, std::move(stream_sender)),
      error_callback_(std::move(error_callback)) {
  stream_sender_.set_disconnect_handler(
      base::BindOnce(&RedirectionRemotingSender::OnRemotingDataStreamError,
                     base::Unretained(this)));
}

RedirectionRemotingSender::~RedirectionRemotingSender() = default;

void RedirectionRemotingSender::SetStream(IMMRStream* stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(stream);
  CHECK(!data_stream_);
  if (!decoder_buffer_reader_) {
    return;
  }
  data_stream_ = stream;

  // Now that there is a stream to write frames to, start reading from the data
  // pipe.
  decoder_buffer_reader_->ReadBufferAsync();
}

void RedirectionRemotingSender::SendFrame(media::mojom::DecoderBufferPtr buffer,
                                          SendFrameCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (read_complete_cb_) {
    // This should never occur if the API is being used as intended, as only
    // one SendFrame() call should be ongoing at a time.
    mojo::ReportBadMessage(
        "Multiple calls made to RemotingDataStreamSender::SendFrame()");
    return;
  }
  read_complete_cb_ = std::move(callback);
  decoder_buffer_reader_->ProvideBuffer(std::move(buffer));
}

void RedirectionRemotingSender::CancelInFlightData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  discard_in_flight_frame_ = !read_complete_cb_.is_null();
}

void RedirectionRemotingSender::OnFrameRead(
    scoped_refptr<media::DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(buffer);
  CHECK(data_stream_);
  CHECK(read_complete_cb_);
  CHECK(!decoder_buffer_reader_->is_read_pending());

  HRESULT hr = S_OK;
  if (discard_in_flight_frame_) {
    // Discarding the current in flight frame and not sending it to the
    // MMRStream. The next frame will be read and sent to the MMRStream.
    discard_in_flight_frame_ = false;
  } else if (buffer->end_of_stream()) {
    hr = data_stream_->EndOfStream();
    if (FAILED(hr)) {
      LOG(ERROR) << "IMMRStream::EndOfStream failed: 0x" << std::hex << hr;
    }
  } else {
    // AppendBuffer() takes the raw encoded bytes alongside their metadata; the
    // sink serializes them for the wire, so they must not be pre-serialized
    // here.
    //
    // MIDL does not emit `const` for `[in]` pointer parameters, so the buffer
    // has to be cast even though the IDL marks it read-only.
    const base::span<const uint8_t> data = base::span(*buffer);
    const MFTIME duration = buffer->duration() != media::kNoTimestamp
                                ? media::TimeDeltaToMfTime(buffer->duration())
                                : 0;
    hr = data_stream_->AppendBuffer(
        const_cast<BYTE*>(data.data()), base::checked_cast<UINT32>(data.size()),
        media::TimeDeltaToMfTime(buffer->timestamp()), duration,
        buffer->is_key_frame());
    if (FAILED(hr)) {
      LOG(ERROR) << "IMMRStream::AppendBuffer failed: 0x" << std::hex << hr;
    }
  }

  if (FAILED(hr)) {
    // The frame is gone and the sink cannot decode past the gap, so end the
    // session rather than acking a frame that was never delivered. The teardown
    // is posted because it destroys this sender, and `decoder_buffer_reader_`
    // modifies its local state after this callback returns.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RedirectionRemotingSender::OnRemotingDataStreamError,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  decoder_buffer_reader_->ReadBufferAsync();
  std::move(read_complete_cb_).Run();
}

void RedirectionRemotingSender::OnRemotingDataStreamError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // NOTE: This method must be idempotent as it may be called more than once.
  decoder_buffer_reader_.reset();
  stream_sender_.reset();
  data_stream_.Reset();
  read_complete_cb_.Reset();
  if (error_callback_) {
    std::move(error_callback_).Run();
  }
}

}  // namespace redirection
