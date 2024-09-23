// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/test/scoped_media_device_info.h"

namespace media_effects {

ScopedMediaDeviceInfo::ScopedMediaDeviceInfo() {
  auto_reset_media_device_info_override_.emplace(
      media_effects::MediaDeviceInfo::OverrideInstanceForTesting());
}

ScopedMediaDeviceInfo::~ScopedMediaDeviceInfo() = default;

}  // namespace media_effects
