// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio/audio_media_data_source.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace ash::libassistant {

namespace {

// The maximum number of bytes to decode on each iteration.
// 512 was chosen to make sure decoding does not block for long.
constexpr uint32_t kMaxBytesToDecode = 512;

}  // namespace

AudioMediaDataSource::AudioMediaDataSource(
    mojo::PendingReceiver<AssistantMediaDataSource> receiver)
    : receiver_(this, std::move(receiver)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      weak_factory_(this) {}

AudioMediaDataSource::~AudioMediaDataSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (read_callback_) {
    // During shutdown, it is possible we received a call to |Read()| but have
    // not received the data from Libassistant yet. In that case we must still
    // call the |read_callback_| to satisfy the mojom API contract.
    OnFillBuffer(0);
  }
}

void AudioMediaDataSource::Read(uint32_t size, ReadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: mojom calls are sequenced, so we should not receive a second call to
  // Read() before we consumed the previous |read_callback_|.
  DCHECK(!read_callback_);
  read_callback_ = std::move(callback);

  if (!delegate_) {
    OnFillBuffer(0);
    return;
  }

  size = std::min(size, kMaxBytesToDecode);
  source_buffer_.resize(size);

  delegate_->FillBuffer(
      source_buffer_.data(), source_buffer_.size(),
      // TODO(wutao): This should be a future time that these buffers would be
      // played.
      base::TimeTicks::Now().since_origin().InMicroseconds(),
      [task_runner = task_runner_,
       weak_ptr = weak_factory_.GetWeakPtr()](int bytes_available) {
        task_runner->PostTask(
            FROM_HERE, base::BindOnce(&AudioMediaDataSource::OnFillBuffer,
                                      weak_ptr, bytes_available));
      });
}

void AudioMediaDataSource::OnFillBuffer(int bytes_filled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_callback_);
  source_buffer_.resize(bytes_filled);
  std::move(read_callback_).Run(source_buffer_);
}

}  // namespace ash::libassistant
