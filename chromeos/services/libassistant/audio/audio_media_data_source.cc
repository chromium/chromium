// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/audio/audio_media_data_source.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/time/time.h"

namespace chromeos {
namespace libassistant {

namespace {

// The maximum number of bytes to decode on each iteration.
// 512 was chosen to make sure decoding does not block for long.
constexpr uint32_t kMaxBytesToDecode = 512;

}  // namespace

AudioMediaDataSource::AudioMediaDataSource(
    mojo::PendingReceiver<AssistantMediaDataSource> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : receiver_(this, std::move(receiver)),
      task_runner_(task_runner),
      weak_factory_(this) {}

AudioMediaDataSource::~AudioMediaDataSource() {
  if (read_callback_) {
    // During shutdown, it is possible we received a call to |Read()| but have
    // not received the data from Libassistant yet. In that case we must still
    // call the |read_callback_| to satisfy the mojom API contract.
    std::move(read_callback_).Run({});
  }
}

void AudioMediaDataSource::Read(uint32_t size, ReadCallback callback) {
  // Note: mojom calls are sequenced, so we should not receive a second call to
  // Read() before we consumed the previous |read_callback_|.
  DCHECK(!read_callback_);
  read_callback_ = std::move(callback);

  if (!delegate_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioMediaDataSource::OnFillBuffer,
                                          weak_factory_.GetWeakPtr(), 0));
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
  DCHECK(read_callback_);
  source_buffer_.resize(bytes_filled);
  std::move(read_callback_).Run(source_buffer_);
}

}  // namespace libassistant
}  // namespace chromeos
