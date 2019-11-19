// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_media_data_source.h"

#include <algorithm>

#include "base/bind.h"
#include "base/time/time.h"

namespace chromeos {
namespace assistant {

namespace {

// The maximum number of bytes to decode on each iteration.
// 512 was chosen to make sure decoding does not block for long.
constexpr uint32_t kMaxBytesToDecode = 512;

}  // namespace

AudioMediaDataSource::AudioMediaDataSource(
    mojo::PendingReceiver<mojom::AssistantMediaDataSource> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : receiver_(this, std::move(receiver)),
      task_runner_(task_runner),
      weak_factory_(this) {}

AudioMediaDataSource::~AudioMediaDataSource() = default;

void AudioMediaDataSource::Read(
    uint32_t size,
    mojom::AssistantMediaDataSource::ReadCallback callback) {
  if (!delegate_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioMediaDataSource::OnFillBuffer,
                       weak_factory_.GetWeakPtr(), std::move(callback), 0));
    return;
  }

  size = std::min(size, kMaxBytesToDecode);
  source_buffer_.resize(size);
  delegate_->FillBuffer(
      source_buffer_.data(), source_buffer_.size(),
      // TODO(wutao): This should be a future time that these buffers would be
      // played.
      base::TimeTicks::Now().since_origin().InMicroseconds(), [
        task_runner = task_runner_, weak_ptr = weak_factory_.GetWeakPtr(),
        repeating_callback =
            base::AdaptCallbackForRepeating(std::move(callback))
      ](int bytes_available) {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&AudioMediaDataSource::OnFillBuffer, weak_ptr,
                           std::move(repeating_callback), bytes_available));
      });
}

void AudioMediaDataSource::OnFillBuffer(
    mojom::AssistantMediaDataSource::ReadCallback callback,
    int bytes_filled) {
  source_buffer_.resize(bytes_filled);
  std::move(callback).Run(source_buffer_);
}

}  // namespace assistant
}  // namespace chromeos
