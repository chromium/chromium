// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_
#define CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/functional/callback.h"
#include "chromecast/public/media/cast_key_system.h"
#include "chromecast/public/media/decrypt_context.h"

namespace chromecast {
namespace media {

// Base class of a decryption context: a decryption context gathers all the
// information needed to decrypt frames with a given key id.
// Each CDM should implement this and add fields needed to fully describe a
// decryption context.
class DecryptContextImpl : public DecryptContext {
 public:
  using DecryptCB = base::OnceCallback<void(bool)>;

  // Type for the output buffer:
  // |kSecure| means the decrypted data will be put in a secured buffer, where
  // normal CPU can't access.
  // |kClearAllowed| means the license allows the decrypted data to be accessed
  // by normal CPU. Caller can decide where is the decrypted data.
  // |kClearRequired| means the CDM or platform doesn't support secure buffer.
  enum class OutputType { kSecure, kClearAllowed, kClearRequired };

  explicit DecryptContextImpl(CastKeySystem key_system);

  DecryptContextImpl(const DecryptContextImpl&) = delete;
  DecryptContextImpl& operator=(const DecryptContextImpl&) = delete;

  ~DecryptContextImpl() override;

  // DecryptContext implementation:
  CastKeySystem GetKeySystem() final;
  bool Decrypt(CastDecoderBuffer* buffer,
               uint8_t* opaque_handle,
               size_t data_offset) final;

  // Similar as the above one. Decryption success or not will be returned in
  // |decrypt_cb|. |output_or_handle| is a pointer to the normal memory, if
  // |clear_output| is true. Otherwise, it's an opaque handle to the secure
  // memory which is only accessible in TEE. |decrypt_cb| will be called on
  // caller's thread.
  virtual void DecryptAsync(CastDecoderBuffer* buffer,
                            uint8_t* output_or_handle,
                            size_t data_offset,
                            bool clear_output,
                            DecryptCB decrypt_cb);

  // Returns the type of output buffer.
  virtual OutputType GetOutputType() const;

 private:
  CastKeySystem key_system_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_
