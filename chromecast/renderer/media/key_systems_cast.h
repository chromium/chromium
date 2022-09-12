// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_KEY_SYSTEMS_CAST_H_
#define CHROMECAST_RENDERER_MEDIA_KEY_SYSTEMS_CAST_H_

#include <memory>
#include <vector>

namespace media {
class KeySystemInfo;
}

namespace chromecast {
namespace media {

void AddChromecastKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemInfo>>*
        key_systems_properties,
    bool enable_persistent_license_support,
    bool enable_playready);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_KEY_SYSTEMS_CAST_H_
