// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_CDM_DECRYPTOR_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_CDM_DECRYPTOR_H_

#include "base/memory/weak_ptr.h"
#include "chromecast/media/cma/pipeline/stream_decryptor.h"

namespace chromecast {
namespace media {

// StreamDecryptor implemented with CDM decrypt APIs.
class CdmDecryptor : public StreamDecryptor {
 public:
  explicit CdmDecryptor(bool clear_buffer_needed);

  CdmDecryptor(const CdmDecryptor&) = delete;
  CdmDecryptor& operator=(const CdmDecryptor&) = delete;

  ~CdmDecryptor() override;

  // StreamDecryptor implementation:
  void Init(const DecryptCB& decrypt_cb) override;
  void Decrypt(scoped_refptr<DecoderBufferBase> buffer) override;

 private:
  void OnResult(scoped_refptr<DecoderBufferBase> buffer, bool success);

  DecryptCB decrypt_cb_;

  const bool clear_buffer_needed_;

  base::WeakPtr<CdmDecryptor> weak_this_;
  base::WeakPtrFactory<CdmDecryptor> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_CDM_DECRYPTOR_H_
