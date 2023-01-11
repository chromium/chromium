// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_WEB_CODECS_DELEGATING_DECODER_BUFFER_PROVIDER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_WEB_CODECS_DELEGATING_DECODER_BUFFER_PROVIDER_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/renderer/public/decoder_buffer_provider.h"

namespace cast_streaming::webcodecs {

// This class provides a thread-safe wrapper around a DecoderBufferProvider's
// weak_ptr, to avoid the limitation that a weak_ptr may only be called from
// the thread where it is created. All calls are delegated to the underlying
// instance.
//
// In practice, an instance of this class will be provided to a javascript
// instance, which will then use it to pull frame data from the
// |delegated_provider_| on a javascript-specific task runner.
template <typename TConfigType>
class DelegatingDecoderBufferProvider
    : public DecoderBufferProvider<TConfigType> {
 public:
  using GetConfigCb = typename DecoderBufferProvider<TConfigType>::GetConfigCb;
  using NewBufferCb = typename DecoderBufferProvider<TConfigType>::NewBufferCb;
  using DeletionCb = typename DecoderBufferProvider<TConfigType>::DeletionCb;

  DelegatingDecoderBufferProvider(
      base::WeakPtr<DecoderBufferProvider<TConfigType>> delegated_provider,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : delegated_provider_(std::move(delegated_provider)),
        task_runner_(std::move(task_runner)),
        weak_factory_(this) {}

  ~DelegatingDecoderBufferProvider() override = default;

  // DecoderBufferProvider overrides.
  bool IsValid() const override { return existed_at_last_check_.load(); }

  void GetConfigAsync(GetConfigCb callback) const override {
    if (!existed_at_last_check_) {
      return;
    }

    if (!task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DelegatingDecoderBufferProvider::GetConfigAsync,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
      return;
    }

    if (!delegated_provider_) {
      existed_at_last_check_.store(false);
      return;
    }

    delegated_provider_->GetConfigAsync(std::move(callback));
  }

  void ReadBufferAsync(NewBufferCb callback) override {
    if (!existed_at_last_check_) {
      return;
    }

    if (!task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DelegatingDecoderBufferProvider::ReadBufferAsync,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
      return;
    }

    if (!delegated_provider_) {
      existed_at_last_check_.store(false);
      return;
    }

    delegated_provider_->ReadBufferAsync(std::move(callback));
  }

  void SetInvalidationCallback(DeletionCb callback) override {
    if (!existed_at_last_check_) {
      std::move(callback).Run();
      return;
    }

    if (!task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &DelegatingDecoderBufferProvider::SetInvalidationCallback,
              weak_factory_.GetWeakPtr(), std::move(callback)));
      return;
    }

    if (!delegated_provider_) {
      existed_at_last_check_.store(false);
      std::move(callback).Run();
      return;
    }

    delegated_provider_->SetInvalidationCallback(std::move(callback));
  }

 private:
  mutable std::atomic_bool existed_at_last_check_{true};
  base::WeakPtr<DecoderBufferProvider<TConfigType>> delegated_provider_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DelegatingDecoderBufferProvider> weak_factory_;
};

}  // namespace cast_streaming::webcodecs

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_WEB_CODECS_DELEGATING_DECODER_BUFFER_PROVIDER_H_
