// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

std::unique_ptr<StarboardApiWrapper> GetStarboardApiWrapper() {
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
