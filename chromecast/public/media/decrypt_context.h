// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_DECRYPT_CONTEXT_H_
#define CHROMECAST_PUBLIC_MEDIA_DECRYPT_CONTEXT_H_

#include <stdint.h>

#include "cast_key_system.h"

namespace chromecast {
namespace media {
class CastDecoderBuffer;

// Provides the information needed to decrypt frames.
class DecryptContext {
 public:
  virtual ~DecryptContext() {}

  // Get the key system to use for decryption.
  virtual CastKeySystem GetKeySystem() = 0;

  // Decrypts the given buffer. Returns true/false for success/failure.
  //
  // |opaque_handle| is a handle to the secure memory, which is only accessible
  // by TEE.
  //
  // The decrypted data will be of size |buffer.size()| and there must be
  // enough space in |opaque_handle| to store that data.
  //
  // If non-zero, |data_offset| specifies an offset to be applied to
  // |opaque_handle| before the decrypted data is written.
  virtual bool Decrypt(CastDecoderBuffer* buffer,
                       uint8_t* opaque_handle,
                       size_t data_offset) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_DECRYPT_CONTEXT_H_
