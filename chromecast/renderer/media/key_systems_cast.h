// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_KEY_SYSTEMS_CAST_H_
#define CHROMECAST_RENDERER_MEDIA_KEY_SYSTEMS_CAST_H_

#include "media/base/key_system_info.h"

namespace chromecast {
namespace media {

void AddChromecastKeySystems(::media::KeySystemInfos* key_system_infos,
                             bool enable_persistent_license_support);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_KEY_SYSTEMS_CAST_H_
