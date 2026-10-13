// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/starboard_adapter_loader.h"

namespace chromecast {
namespace media {

// Proxy implementation for GetStarboardApiWrapper().
//
// Routes media pipeline wrapper requests to StarboardAdapterLoader, which
// delegates creation to the dynamically loaded versioned adapter library.
std::unique_ptr<StarboardApiWrapper> GetStarboardApiWrapper() {
  auto* wrapper = StarboardAdapterLoader::GetInstance()->CreateMediaWrapper();
  if (!wrapper) {
    LOG(ERROR)
        << "Failed to create StarboardApiWrapper instance from adapter loader.";
    return nullptr;
  }
  return base::WrapUnique(wrapper);
}

}  // namespace media
}  // namespace chromecast
