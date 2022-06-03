// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/cdm_decryptor.h"

#include "base/bind.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/base/decrypt_context_impl.h"
#include "chromecast/media/cma/pipeline/decrypt_util.h"

namespace chromecast {
namespace media {

CdmDecryptor::CdmDecryptor(bool clear_buffer_needed)
    : clear_buffer_needed_(clear_buffer_needed), weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

CdmDecryptor::~CdmDecryptor() = default;

void CdmDecryptor::Init(const DecryptCB& decrypt_cb) {
  DCHECK(!decrypt_cb_);
  decrypt_cb_ = decrypt_cb;
}

void CdmDecryptor::Decrypt(scoped_refptr<DecoderBufferBase> buffer) {
  if (buffer->end_of_stream() || !buffer->decrypt_config()) {
    OnResult(std::move(buffer), true);
    return;
  }

  DecryptContextImpl* decrypt_context =
      static_cast<DecryptContextImpl*>(buffer->decrypt_context());
  DCHECK(decrypt_context);

  DecryptContextImpl::OutputType output_type = decrypt_context->GetOutputType();
  if (output_type == DecryptContextImpl::OutputType::kClearRequired ||
      (clear_buffer_needed_ &&
       output_type == DecryptContextImpl::OutputType::kClearAllowed)) {
    DecryptDecoderBuffer(std::move(buffer), decrypt_context,
                         base::BindOnce(&CdmDecryptor::OnResult, weak_this_));
    return;
  }

  // Media pipeline backend will handle decryption.
  OnResult(std::move(buffer), true);
}

void CdmDecryptor::OnResult(scoped_refptr<DecoderBufferBase> buffer,
                            bool success) {
  BufferQueue ready_buffers;
  ready_buffers.push(buffer);

  DCHECK(decrypt_cb_);
  decrypt_cb_.Run(success, std::move(ready_buffers));
}

}  // namespace media
}  // namespace chromecast
