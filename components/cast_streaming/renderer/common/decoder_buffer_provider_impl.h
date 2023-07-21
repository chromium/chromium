// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_COMMON_DECODER_BUFFER_PROVIDER_IMPL_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_COMMON_DECODER_BUFFER_PROVIDER_IMPL_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/renderer/public/decoder_buffer_provider.h"
#include "media/base/decoder_buffer.h"
#include "media/cast/openscreen/decoder_buffer_reader.h"
#include "media/mojo/mojom/media_types.mojom-forward.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace cast_streaming {

// This class provides a synchronous implementation of DecoderBufferProvider,
// to be used for processing decoder buffers with respect to a single config.
template <typename TConfigType>
class DecoderBufferProviderImpl : public DecoderBufferProvider<TConfigType> {
 public:
  using NewBufferCb = typename DecoderBufferProvider<TConfigType>::NewBufferCb;
  using GetConfigCb = typename DecoderBufferProvider<TConfigType>::GetConfigCb;
  using DeletionCb = typename DecoderBufferProvider<TConfigType>::DeletionCb;

  class Client {
   public:
    virtual ~Client() = default;

    // Requests a new buffer be provided to this class, which will be returned
    // via call to |on_buffer_received|.
    using BufferProviderRequestCB =
        base::OnceCallback<void(media::mojom::DecoderBufferPtr)>;
    virtual void RequestBufferAsync(
        BufferProviderRequestCB n_buffer_received) = 0;
  };

  DecoderBufferProviderImpl(
      TConfigType config,
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      Client* client,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : config_(std::move(config)),
        client_(client),
        task_runner_(std::move(task_runner)),
        weak_factory_(this) {
    DCHECK(task_runner_);
    DCHECK(client_);
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    buffer_reader_ = std::make_unique<media::cast::DecoderBufferReader>(
        base::BindRepeating(
            &DecoderBufferProviderImpl<TConfigType>::OnBufferRead,
            weak_factory_.GetWeakPtr()),
        std::move(data_pipe));
  }

  ~DecoderBufferProviderImpl() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (deletion_callback_) {
      std::move(deletion_callback_).Run();
    }
  }

  base::WeakPtr<DecoderBufferProviderImpl<TConfigType>> GetWeakPtr() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    return weak_factory_.GetWeakPtr();
  }

  // DecoderBufferProvider overrides.
  bool IsValid() const override { return true; }

  void GetConfigAsync(GetConfigCb callback) const override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    std::move(callback).Run(config_);
  }

  void ReadBufferAsync(NewBufferCb callback) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!new_buffer_callback_);
    new_buffer_callback_ = std::move(callback);
    RequestBuffer();
  }

  void SetInvalidationCallback(DeletionCb callback) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    deletion_callback_ = std::move(callback);
  }

 private:
  // Provided as the callback to Client::RequestBufferAsync().
  void OnMojoBufferReceived(media::mojom::DecoderBufferPtr buffer) {
    if (!task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DecoderBufferProviderImpl::OnMojoBufferReceived,
                         weak_factory_.GetWeakPtr(), std::move(buffer)));
      return;
    }

    DCHECK(new_buffer_callback_);
    if (!buffer) {
      buffer_reader_->ClearReadPending();
      std::move(new_buffer_callback_).Run(nullptr);
      return;
    }
    buffer_reader_->ProvideBuffer(std::move(buffer));
  }

  // Callback provided to |buffer_reader_| to be called when a new |buffer| is
  // available for processing.
  void OnBufferRead(scoped_refptr<media::DecoderBuffer> buffer) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(new_buffer_callback_);
    std::move(new_buffer_callback_).Run(std::move(buffer));
  }

  // Requests a new buffer be provided at some point in the future.
  void RequestBuffer() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    buffer_reader_->ReadBufferAsync();
    client_->RequestBufferAsync(base::BindOnce(
        &DecoderBufferProviderImpl<TConfigType>::OnMojoBufferReceived,
        weak_factory_.GetWeakPtr()));
  }

  const TConfigType config_;
  NewBufferCb new_buffer_callback_;
  std::unique_ptr<media::cast::DecoderBufferReader> buffer_reader_;
  DeletionCb deletion_callback_;
  raw_ptr<Client> client_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<DecoderBufferProviderImpl> weak_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_COMMON_DECODER_BUFFER_PROVIDER_IMPL_H_
