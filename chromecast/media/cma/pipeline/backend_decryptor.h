// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_BACKEND_DECRYPTOR_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_BACKEND_DECRYPTOR_H_

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/pipeline/stream_decryptor.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
class TaskRunnerImpl;

namespace media {

class BackendDecryptor : public StreamDecryptor,
                         MediaPipelineBackend::AudioDecryptor::Delegate {
 public:
  explicit BackendDecryptor(EncryptionScheme scheme);
  ~BackendDecryptor() override;

  // StreamDecryptor implementation:
  void Init(const DecryptCB& decrypt_cb) override;
  void Decrypt(scoped_refptr<DecoderBufferBase> buffer) override;

 private:
  // MediaPipelineBackend::AudioDecryptor::Delegate implementation:
  void OnPushBufferForDecryptComplete(
      MediaPipelineBackend::BufferStatus status) override;
  void OnDecryptComplete(bool success) override;

  // Pending buffers for decrypt.
  BufferQueue pending_buffers_;

  // Buffers that are ready to return to caller.
  BufferQueue ready_buffers_;
  bool decrypt_success_;
  bool wait_eos_;

  // |task_runner_| should have a longer life than |decryptor_|.
  std::unique_ptr<TaskRunnerImpl> task_runner_;
  std::unique_ptr<MediaPipelineBackend::AudioDecryptor> decryptor_;

  DecryptCB decrypt_cb_;

  DISALLOW_COPY_AND_ASSIGN(BackendDecryptor);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_BACKEND_DECRYPTOR_H_
