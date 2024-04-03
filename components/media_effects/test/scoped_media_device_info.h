// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_TEST_SCOPED_MEDIA_DEVICE_INFO_H_
#define COMPONENTS_MEDIA_EFFECTS_TEST_SCOPED_MEDIA_DEVICE_INFO_H_

#include "components/media_effects/media_device_info.h"

namespace media_effects {

class ScopedMediaDeviceInfo {
 public:
  ScopedMediaDeviceInfo();
  ~ScopedMediaDeviceInfo();

  ScopedMediaDeviceInfo(const ScopedMediaDeviceInfo&) = delete;
  ScopedMediaDeviceInfo& operator=(const ScopedMediaDeviceInfo&) = delete;

 private:
  base::SystemMonitor monitor_;
  std::optional<std::pair<std::unique_ptr<media_effects::MediaDeviceInfo>,
                          base::AutoReset<media_effects::MediaDeviceInfo*>>>
      auto_reset_media_device_info_override_;
};

}  // namespace media_effects

#endif  // COMPONENTS_MEDIA_EFFECTS_TEST_SCOPED_MEDIA_DEVICE_INFO_H_
