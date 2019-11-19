// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/backend_decryptor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/pipeline/decrypt_util.h"

namespace chromecast {
namespace media {

BackendDecryptor::BackendDecryptor(EncryptionScheme scheme)
    : decrypt_success_(true),
      wait_eos_(false),
      task_runner_(new TaskRunnerImpl) {
  DCHECK(MediaPipelineBackend::CreateAudioDecryptor);

  task_runner_ = std::make_unique<TaskRunnerImpl>();
  decryptor_ = base::WrapUnique(
      MediaPipelineBackend::CreateAudioDecryptor(scheme, task_runner_.get()));

  DCHECK(decryptor_);
  decryptor_->SetDelegate(this);
}

BackendDecryptor::~BackendDecryptor() = default;

void BackendDecryptor::Init(const DecryptCB& decrypt_cb) {
  DCHECK(!decrypt_cb_);
  decrypt_cb_ = decrypt_cb;
}

void BackendDecryptor::Decrypt(scoped_refptr<DecoderBufferBase> buffer) {
  DCHECK(!wait_eos_);

  // Push both clear and encrypted buffers to backend, so that |decryptor_|
  // won't be blocked if there are not enough encrypted buffers. EOS buffer is
  // also needed so that the last buffer can be flushed.
  pending_buffers_.push(buffer);

  if (buffer->end_of_stream())
    wait_eos_ = true;

  MediaPipelineBackend::BufferStatus status = decryptor_->PushBufferForDecrypt(
      buffer.get(),
      buffer->end_of_stream() ? nullptr : buffer->writable_data());

  if (status != MediaPipelineBackend::kBufferPending)
    OnPushBufferForDecryptComplete(status);
}

void BackendDecryptor::OnPushBufferForDecryptComplete(
    MediaPipelineBackend::BufferStatus status) {
  // If the pushed buffer is EOS, the callback should be called when all the
  // buffers are decrypted.
  if (wait_eos_)
    return;

  DCHECK(decrypt_cb_);
  decrypt_cb_.Run(
      decrypt_success_ && status == MediaPipelineBackend::kBufferSuccess,
      std::move(ready_buffers_));
}

void BackendDecryptor::OnDecryptComplete(bool success) {
  DCHECK(!pending_buffers_.empty());

  // Cache the success value and return it in OnPushBufferForDecryptComplete.
  decrypt_success_ &= success;

  scoped_refptr<DecoderBufferBase> buffer = std::move(pending_buffers_.front());
  pending_buffers_.pop();

  ready_buffers_.push(buffer->end_of_stream() || !buffer->decrypt_config()
                          ? buffer
                          : base::MakeRefCounted<DecoderBufferClear>(buffer));

  if (wait_eos_ && buffer->end_of_stream()) {
    // Last frame, all the buffers should be decrypted.
    DCHECK(pending_buffers_.empty());
    DCHECK(decrypt_cb_);
    LOG(INFO) << "Return all the ready buffers, size = "
              << ready_buffers_.size();
    decrypt_cb_.Run(decrypt_success_, std::move(ready_buffers_));
  }
}

}  // namespace media
}  // namespace chromecast
