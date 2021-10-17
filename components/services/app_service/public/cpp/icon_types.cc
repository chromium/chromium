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
