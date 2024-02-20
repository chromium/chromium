// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_COMMON_BUFFER_REQUESTER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_COMMON_BUFFER_REQUESTER_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/common/frame/demuxer_stream_traits.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/renderer/common/decoder_buffer_provider_impl.h"
#include "components/cast_streaming/renderer/public/decoder_buffer_provider.h"
#include "media/mojo/mojom/media_types.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace cast_streaming {

template <typename TMojoRemoteType>
class BufferRequester;

using AudioBufferRequester = BufferRequester<mojom::AudioBufferRequester>;
using VideoBufferRequester = BufferRequester<mojom::VideoBufferRequester>;

// This class provides a wrapper around the remote end of a BufferRequester
// mojo API (either AudioBufferRequester or VideoBufferRequester). It handles
// creating new EncodedBufferProviders as new configs are available, as well as
// requesting buffers for such instances.
template <typename TMojoRemoteType>
class BufferRequester
    : public DemuxerStreamTraits<TMojoRemoteType>,
      public DecoderBufferProviderImpl<
          typename DemuxerStreamTraits<TMojoRemoteType>::ConfigType>::Client {
 public:
  // See DemuxerStreamTraits for further details on these types.
  using Traits = DemuxerStreamTraits<TMojoRemoteType>;
  using ConfigType = typename Traits::ConfigType;

  class Client {
   public:
    virtual ~Client() = default;

    virtual void OnNewBufferProvider(
        base::WeakPtr<DecoderBufferProvider<ConfigType>> ptr) = 0;
    virtual void OnMojoDisconnect() = 0;
  };

  BufferRequester(Client* client,
                  ConfigType config,
                  mojo::ScopedDataPipeConsumerHandle data_pipe,
                  mojo::PendingRemote<TMojoRemoteType> pending_remote,
                  scoped_refptr<base::SequencedTaskRunner> task_runner)
      : client_(client),
        task_runner_(std::move(task_runner)),
        remote_(std::move(pending_remote)),
        weak_factory_(this) {
    DCHECK(client_);
    DCHECK(task_runner_);

    // Unretained is safe here because |client| is expected to outlive this
    // instance.
    remote_.set_disconnect_handler(
        base::BindOnce(&Client::OnMojoDisconnect, base::Unretained(client)));
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BufferRequester<TMojoRemoteType>::OnNewConfig,
                       weak_factory_.GetWeakPtr(), std::move(config),
                       std::move(data_pipe)));
  }

  ~BufferRequester() override = default;

  // DecoderBufferProviderImpl::Client implementation.
  using BufferProviderRequestCB =
      typename DecoderBufferProviderImpl<typename DemuxerStreamTraits<
          TMojoRemoteType>::ConfigType>::Client::BufferProviderRequestCB;
  void RequestBufferAsync(BufferProviderRequestCB on_buffer_received) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!buffer_request_cb_);
    buffer_request_cb_ = std::move(on_buffer_received);
    remote_->GetBuffer(base::BindOnce(&BufferRequester::OnGetBufferDone,
                                      weak_factory_.GetWeakPtr()));
  }

  // |callback| is called upon completion of the async call, with result true
  // if the call succeeded and false in all other cases.
  void EnableBitstreamConverterAsync(base::OnceCallback<void(bool)> callback) {
    remote_->EnableBitstreamConverter(std::move(callback));
  }

 private:
  // Processes a new buffer as received over mojo.
  void OnGetBufferDone(
      typename Traits::GetBufferResponseType get_buffer_response) {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(buffer_provider_);
    DCHECK(buffer_request_cb_);

    if (!get_buffer_response) {
      std::move(buffer_request_cb_).Run(nullptr);
      return;
    }

    if (get_buffer_response->is_stream_info()) {
      buffer_request_cb_ = BufferProviderRequestCB{};
      typename Traits::StreamInfoType& data_stream_info =
          get_buffer_response->get_stream_info();
      OnNewConfig(std::move(data_stream_info->decoder_config),
                  std::move(data_stream_info->data_pipe));
    } else {
      DCHECK(get_buffer_response->is_buffer());
      std::move(buffer_request_cb_)
          .Run(std::move(get_buffer_response->get_buffer()));
    }
  }

  // Re-create |buffer_provider_| when a new config is received.
  void OnNewConfig(ConfigType config,
                   mojo::ScopedDataPipeConsumerHandle data_pipe) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    buffer_provider_ = std::make_unique<DecoderBufferProviderImpl<ConfigType>>(
        std::move(config), std::move(data_pipe), this, task_runner_);
    client_->OnNewBufferProvider(buffer_provider_->GetWeakPtr());
  }

  std::unique_ptr<DecoderBufferProviderImpl<ConfigType>> buffer_provider_;
  BufferProviderRequestCB buffer_request_cb_;

  const raw_ptr<Client> client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  mojo::Remote<TMojoRemoteType> remote_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BufferRequester<TMojoRemoteType>> weak_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_COMMON_BUFFER_REQUESTER_H_
