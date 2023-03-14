// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_FRAME_DEMUXER_STREAM_DATA_PROVIDER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_FRAME_DEMUXER_STREAM_DATA_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/cast_streaming/browser/common/demuxer_stream_client.h"
#include "components/cast_streaming/common/frame/demuxer_stream_traits.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace cast_streaming {

// Forward declarations of concrete types. Definitions to follow.
template <typename TMojoReceiverType>
class DemuxerStreamDataProvider;

using AudioDemuxerStreamDataProvider =
    DemuxerStreamDataProvider<mojom::AudioBufferRequester>;
using VideoDemuxerStreamDataProvider =
    DemuxerStreamDataProvider<mojom::VideoBufferRequester>;

// Helper class to simplify responding to calls made over AudioBufferRequester
// and VideoBufferRequester mojo APIs.
//
// |TMojoReceiverType| is the interface used for requesting data buffers.
// Currently expected to be either AudioBufferRequester or VideoBufferRequester.
template <typename TMojoReceiverType>
class DemuxerStreamDataProvider : public DemuxerStreamTraits<TMojoReceiverType>,
                                  public TMojoReceiverType {
 public:
  // See DemuxerStreamTraits for further details on these types.
  using Traits = DemuxerStreamTraits<TMojoReceiverType>;
  using GetBufferResponseType = typename Traits::GetBufferResponseType;
  using StreamInfoType = typename Traits::StreamInfoType;
  using ConfigType = typename Traits::ConfigType;

  // The callback type which will be used to request a new buffer be read. The
  // callback is expected to call ProvideBuffer() once a buffer is available.
  // The callback parameter provided when calling a RequestBufferCB is to be
  // called if there no buffers available for reading at time of calling.
  using RequestBufferCB = base::RepeatingCallback<void(base::OnceClosure)>;

  // |request_buffer| is the callback which will be used to request a new
  // buffer be read. The callback is expected to call ProvideBuffer() once a
  // buffer is available.
  DemuxerStreamDataProvider(
      mojo::PendingReceiver<TMojoReceiverType> pending_receiver,
      RequestBufferCB request_buffer,
      base::OnceClosure on_mojo_disconnect,
      ConfigType config)
      : config_(std::move(config)),
        request_buffer_(std::move(request_buffer)),
        on_mojo_disconnect_(std::move(on_mojo_disconnect)),
        receiver_(this, std::move(pending_receiver)),
        weak_factory_(this) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &DemuxerStreamDataProvider::OnFatalError, weak_factory_.GetWeakPtr()));
  }

  // Sets the new config to be passed to the renderer process as part of the
  // response to the ongoing GetBuffer() request.
  void OnNewStreamInfo(ConfigType config,
                       mojo::ScopedDataPipeConsumerHandle handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    is_new_stream_info_pending_ = false;
    config_ = std::move(config);
    next_stream_info_ =
        StreamInfoType::element_type::New(config_, std::move(handle));
    if (current_callback_) {
      std::move(current_callback_)
          .Run(GetBufferResponseType::element_type::NewStreamInfo(
              std::move(next_stream_info_)));
    }
  }

  // Stops reading frames until a new StreamInfo has been applied by a call to
  // OnNewStreamInfo().
  void WaitForNewStreamInfo() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    is_new_stream_info_pending_ = true;
    if (current_callback_) {
      std::move(current_callback_).Run(nullptr);
    }
  }

  // Sets the buffer to be passed to the renderer process as part of the
  // response to the ongoing GetBuffer() request.
  void ProvideBuffer(media::mojom::DecoderBufferPtr buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Only occurs on a FlushUntil() call. If preloading is ongoing, that will
    // occur as part of the next GetBuffer() call.
    if (!buffer) {
      if (current_callback_) {
        std::move(current_callback_).Run(nullptr);
      }
      return;
    }

    if (preload_buffer_cb_) {
      std::move(preload_buffer_cb_).Run(std::move(buffer));
      if (current_callback_) {
        request_buffer_.Run(base::BindOnce(
            &DemuxerStreamClient::OnNoBuffersAvailable, client_));
      }
      return;
    }

    DCHECK(current_callback_);
    DCHECK(!next_stream_info_);
    std::move(current_callback_)
        .Run(GetBufferResponseType::element_type::NewBuffer(std::move(buffer)));
  }

  const ConfigType& config() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return config_;
  }

  void SetClient(base::WeakPtr<DemuxerStreamClient> client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    client_ = std::move(client);
  }

  // Pre-loads a buffer before receiving any calls from the DemuxerStream, then
  // returns it via |callback|.
  using PreloadBufferCB =
      base::OnceCallback<void(media::mojom::DecoderBufferPtr)>;
  void PreloadBuffer(PreloadBufferCB callback) {
    // Check if another preload call is already in progress. This should only
    // occur if a new config was received before the old preload call could be
    // completed.
    if (preload_buffer_cb_) {
      DLOG(WARNING) << "Overwriting previous preload_buffer_cb_";
    }

    DCHECK(!current_callback_);
    preload_buffer_cb_ = std::move(callback);
    request_buffer_.Run(
        base::BindOnce(&DemuxerStreamClient::OnNoBuffersAvailable, client_));
  }

 private:
  using EnableBitstreamConverterCallback =
      typename TMojoReceiverType::EnableBitstreamConverterCallback;
  using GetBufferCallback = typename TMojoReceiverType::GetBufferCallback;

  void OnFatalError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (client_) {
      client_->OnError();
    }
    std::move(on_mojo_disconnect_).Run();
  }

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
    if (is_new_stream_info_pending_) {
      std::move(current_callback_).Run(nullptr);
      return;
    }

    if (next_stream_info_) {
      std::move(current_callback_)
          .Run(GetBufferResponseType::element_type::NewStreamInfo(
              std::move(next_stream_info_)));
      return;
    }

    // If preloading is already ongoing, then a new buffer request isn't needed.
    // Instead just continue the preloading callback which will call this
    // upon completion.
    if (!preload_buffer_cb_) {
      request_buffer_.Run(
          base::BindOnce(&DemuxerStreamClient::OnNoBuffersAvailable, client_));
    }
  }

  void EnableBitstreamConverter(
      EnableBitstreamConverterCallback callback) override {
    if (client_) {
      client_->EnableBitstreamConverter(std::move(callback));
    } else {
      std::move(callback).Run(true);
      LOG(WARNING)
          << "EnableBitstreamConverter() called when no client was available";
    }
  }

  // Set by WaitForNewStreamInfo() to signify that a new StreamInfo is expected
  // and all GetBuffer() calls prior to this change should be blocked.
  bool is_new_stream_info_pending_ = false;

  PreloadBufferCB preload_buffer_cb_;

  // The most recently set config.
  ConfigType config_;

  // The stream info to be sent to the remote upon its next GetBuffer() call,
  // or empty.
  StreamInfoType next_stream_info_;

  // The callback associated with the most recent GetBuffer() call.
  GetBufferCallback current_callback_;

  // Callback to request a new buffer be read from the receiver.
  RequestBufferCB request_buffer_;

  // Client to use when the associated DemuxerStream requires an action be
  // performed.
  base::WeakPtr<DemuxerStreamClient> client_;

  // Callback called upon a mojo disconnection.
  base::OnceClosure on_mojo_disconnect_;

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<TMojoReceiverType> receiver_;

  base::WeakPtrFactory<DemuxerStreamDataProvider> weak_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_FRAME_DEMUXER_STREAM_DATA_PROVIDER_H_
