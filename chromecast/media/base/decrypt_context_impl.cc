// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/decrypt_context_impl.h"

#include <memory>
#include <ostream>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {
namespace {
void BufferDecryptCB(bool* called, bool* ret, bool success) {
  DCHECK(called);
  DCHECK(ret);
  *called = true;
  *ret = success;
}
}

DecryptContextImpl::DecryptContextImpl(CastKeySystem key_system)
    : key_system_(key_system) {}

DecryptContextImpl::~DecryptContextImpl() {}

CastKeySystem DecryptContextImpl::GetKeySystem() {
  return key_system_;
}

bool DecryptContextImpl::Decrypt(CastDecoderBuffer* buffer,
                                 uint8_t* opaque_handle,
                                 size_t data_offset) {
  bool called = false;
  bool success = false;
  DecryptAsync(buffer, opaque_handle, data_offset, false /* clear_output */,
               base::BindOnce(&BufferDecryptCB, &called, &success));
  CHECK(called) << "Sync Decrypt isn't supported";

  return success;
}

void DecryptContextImpl::DecryptAsync(CastDecoderBuffer* buffer,
                                      uint8_t* output_or_handle,
                                      size_t data_offset,
                                      bool clear_output,
                                      DecryptCB decrypt_cb) {
  std::move(decrypt_cb).Run(false);
}

DecryptContextImpl::OutputType DecryptContextImpl::GetOutputType() const {
  return OutputType::kSecure;
}

}  // namespace media
}  // namespace chromecast
