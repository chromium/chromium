// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/cast_cdm_context.h"

namespace chromecast {
namespace media {

::media::Decryptor* CastCdmContext::GetDecryptor() {
  // Subclasses providing CdmContext for a ClearKey CDM implementation must
  // override this method to provide the Decryptor. Subclasses providing DRM
  // implementations should return nullptr here.
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
