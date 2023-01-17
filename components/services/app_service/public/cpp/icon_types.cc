// Copyright 2021 The Chromium Authors
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

bool IconKey::operator==(const IconKey& other) const {
  return timeline == other.timeline && resource_id == other.resource_id &&
         icon_effects == other.icon_effects;
}

bool IconKey::operator!=(const IconKey& other) const {
  return !(*this == other);
}

IconKeyPtr IconKey::Clone() const {
  auto icon_key =
      std::make_unique<IconKey>(timeline, resource_id, icon_effects);
  icon_key->raw_icon_updated = raw_icon_updated;
  return icon_key;
}

constexpr uint64_t IconKey::kDoesNotChangeOverTime = 0;
const int32_t IconKey::kInvalidResourceId = 0;

IconValue::IconValue() = default;
IconValue::~IconValue() = default;

}  // namespace apps
