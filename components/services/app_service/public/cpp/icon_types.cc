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
  return std::make_unique<IconKey>(timeline, resource_id, icon_effects);
}

constexpr uint64_t IconKey::kDoesNotChangeOverTime = 0;
const int32_t IconKey::kInvalidResourceId = 0;

IconValue::IconValue() = default;
IconValue::~IconValue() = default;

apps::mojom::IconKeyPtr ConvertIconKeyToMojomIconKey(const IconKey& icon_key) {
  auto mojom_icon_key = apps::mojom::IconKey::New();
  mojom_icon_key->timeline = icon_key.timeline;
  mojom_icon_key->resource_id = icon_key.resource_id;
  mojom_icon_key->icon_effects = icon_key.icon_effects;
  return mojom_icon_key;
}

std::unique_ptr<IconKey> ConvertMojomIconKeyToIconKey(
    const apps::mojom::IconKeyPtr& mojom_icon_key) {
  DCHECK(mojom_icon_key);
  return std::make_unique<IconKey>(mojom_icon_key->timeline,
                                   mojom_icon_key->resource_id,
                                   mojom_icon_key->icon_effects);
}

apps::mojom::IconType ConvertIconTypeToMojomIconType(IconType icon_type) {
  switch (icon_type) {
    case IconType::kUnknown:
      return apps::mojom::IconType::kUnknown;
    case IconType::kUncompressed:
      return apps::mojom::IconType::kUncompressed;
    case IconType::kCompressed:
      return apps::mojom::IconType::kCompressed;
    case IconType::kStandard:
      return apps::mojom::IconType::kStandard;
  }
}

IconType ConvertMojomIconTypeToIconType(apps::mojom::IconType mojom_icon_type) {
  switch (mojom_icon_type) {
    case apps::mojom::IconType::kUnknown:
      return apps::IconType::kUnknown;
    case apps::mojom::IconType::kUncompressed:
      return apps::IconType::kUncompressed;
    case apps::mojom::IconType::kCompressed:
      return apps::IconType::kCompressed;
    case apps::mojom::IconType::kStandard:
      return apps::IconType::kStandard;
  }
}

}  // namespace apps
