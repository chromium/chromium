// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_DEMUXER_STREAM_DATA_PROVIDER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_DEMUXER_STREAM_DATA_PROVIDER_H_

#include "base/callback_forward.h"
#include "base/sequence_checker.h"
#include "components/cast_streaming/public/mojom/cast_streaming_session.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace cast_streaming {

// Forward declarations of concrete types. Definitions to follow.
template <typename TMojoReceiverType, typename TStreamInfoType>
class DemuxerStreamDataProvider;

using AudioDemuxerStreamDataProvider =
    DemuxerStreamDataProvider<mojom::AudioBufferRequester,
                              mojom::AudioStreamInfoPtr>;
using VideoDemuxerStreamDataProvider =
    DemuxerStreamDataProvider<mojom::VideoBufferRequester,
                              mojom::VideoStreamInfoPtr>;

// Helper class to simplify responding to calls made over AudioBufferRequester
// and VideoBufferRequester mojo APIs.
//
// |TMojoReceiverType| is the interface used for requesting data buffers.
// Currently expected to be either AudioBufferRequester or VideoBufferRequester.
// |TStreamInfoType| is the StreamInfo that may be returned by this call, either
// AudioStreamInfo or VideoStreamInfo.
template <typename TMojoReceiverType, typename TStreamInfoType>
class DemuxerStreamDataProvider : public TMojoReceiverType {
 public:
  // Deduce the Config type associated with this Mojo API (either
  // media::AudioDecoderConfig or media::VideoDecoderConfig).
  typedef decltype(TStreamInfoType::element_type::decoder_config) ConfigType;

  // |request_buffer| is the callback which will be used to request a new
  // buffer be read. The callback is expected to call ProvideBuffer() once a
  // buffer is available.
  DemuxerStreamDataProvider(
      mojo::PendingReceiver<TMojoReceiverType> pending_receiver,
      base::RepeatingClosure request_buffer,
      base::OnceClosure on_mojo_disconnect,
      ConfigType config)
      : config_(std::move(config)),
        request_buffer_(std::move(request_buffer)),
        receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(std::move(on_mojo_disconnect));
  }

  // Sets the new config to be passed to the renderer process as part of the
  // response to the ongoing GetBuffer() request.
  void OnNewStreamInfo(ConfigType config,
                       mojo::ScopedDataPipeConsumerHandle handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    config_ = std::move(config);
    next_stream_info_ =
        TStreamInfoType::element_type::New(config_, std::move(handle));
  }

  // Sets the buffer to be passed to the renderer process as part of the
  // response to the ongoing GetBuffer() request.
  void ProvideBuffer(media::mojom::DecoderBufferPtr buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(current_callback_);

    // NOTE: |next_stream_info_| may be empty if none has been set since the
    // last time this method was called.
    std::move(current_callback_)
        .Run(std::move(next_stream_info_), std::move(buffer));
  }

  const ConfigType& config() const { return config_; }

 private:
  using GetBufferCallback = typename TMojoReceiverType::GetBufferCallback;

  // TMojoReceiverType implementation.
  void GetBuffer(GetBufferCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (current_callback_) {
      // This should never occur if the API is being used as intended, as only
      // one GetBuffer() call should be ongoing at a time.
      mojo::ReportBadMessage(
          "Multiple calls made to BufferRequester::GetBuffer()");
      return;
    }

    current_callback_ = std::move(callback);
    request_buffer_.Run();
  }

  // The most recently set config.
  ConfigType config_;

  // The stream info to be sent to the remote upon its next GetBuffer() call,
  // or empty.
  TStreamInfoType next_stream_info_;

  // The callback associated with the most recent GetBuffer() call.
  GetBufferCallback current_callback_;

  // Callback to request a new buffer be read from the receiver.
  base::RepeatingClosure request_buffer_;

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<TMojoReceiverType> receiver_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_DEMUXER_STREAM_DATA_PROVIDER_H_
