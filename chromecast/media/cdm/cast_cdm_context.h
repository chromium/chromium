// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_CAST_CDM_CONTEXT_H_
#define CHROMECAST_MEDIA_CDM_CAST_CDM_CONTEXT_H_

#include <memory>
#include <string>

#include "chromecast/public/media/cast_decrypt_config.h"
#include "chromecast/public/media/cast_key_status.h"
#include "media/base/cdm_context.h"

namespace chromecast {
namespace media {

class DecryptContextImpl;

// CdmContext implementation + some extra APIs needed by CastRenderer.
class CastCdmContext : public ::media::CdmContext {
 public:
  // ::media::CdmContext implementation.
  ::media::Decryptor* GetDecryptor() override;

  // Returns the decryption context needed to decrypt frames encrypted with
  // |key_id|. Returns null if |key_id| is not available.
  virtual std::unique_ptr<DecryptContextImpl> GetDecryptContext(
      const std::string& key_id,
      EncryptionScheme encryption_scheme) = 0;

  // Notifies that key status has changed (e.g. if expiry is detected by
  // hardware decoder).
  virtual void SetKeyStatus(const std::string& key_id,
                            CastKeyStatus key_status,
                            uint32_t system_code) = 0;

  // Notifies of current decoded video resolution (used for licence policy
  // enforcement).
  virtual void SetVideoResolution(int width, int height) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_CAST_CDM_CONTEXT_H_
