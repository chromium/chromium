// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/icon_types.h"

namespace apps {

IconKey::IconKey() = default;

IconKey::IconKey(uint64_t timeline, int32_t resource_id, uint32_t icon_effects)
    : timeline(timeline),
      resource_id(resource_id),
      icon_effects(icon_effects) {}

IconKey::~IconKey() = default;

IconValue::IconValue() {}

IconValue::~IconValue() {}

}  // namespace apps
