// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include "components/content_settings/core/common/content_settings_types.mojom.h"

// The enum is defined in content_settings_types.mojo.
using ContentSettingsType = content_settings::mojom::ContentSettingsType;

struct ContentSettingsTypeHash {
  size_t operator()(ContentSettingsType type) const {
    return static_cast<size_t>(type);
  }
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_
