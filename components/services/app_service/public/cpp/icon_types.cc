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

IconValue::IconValue() = default;
IconValue::~IconValue() = default;

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

apps::mojom::IconValuePtr ConvertIconValueToMojomIconValue(
    std::unique_ptr<IconValue> icon_value) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  if (!icon_value || icon_value->icon_type == IconType::kUnknown) {
    return iv;
  }

  iv->icon_type = ConvertIconTypeToMojomIconType(icon_value->icon_type);
  iv->is_placeholder_icon = icon_value->is_placeholder_icon;

  switch (icon_value->icon_type) {
    case IconType::kUnknown:
      break;
    case IconType::kCompressed:
      // For a compressed icon, the uncompressed image might be used to apply
      // icon effects, so both `uncompressed` and `compressed` need to be
      // copied.
      iv->uncompressed = icon_value->uncompressed;
      iv->compressed = std::move(icon_value->compressed);
      break;
    case IconType::kUncompressed:
    case IconType::kStandard:
      iv->uncompressed = icon_value->uncompressed;
      break;
  }

  return iv;
}

std::unique_ptr<IconValue> ConvertMojomIconValueToIconValue(
    apps::mojom::IconValuePtr mojom_icon_value) {
  std::unique_ptr<IconValue> iv = std::make_unique<IconValue>();
  if (!mojom_icon_value) {
    return iv;
  }

  iv->icon_type = ConvertMojomIconTypeToIconType(mojom_icon_value->icon_type);
  iv->is_placeholder_icon = mojom_icon_value->is_placeholder_icon;

  switch (mojom_icon_value->icon_type) {
    case mojom::IconType::kUnknown:
      break;
    case mojom::IconType::kCompressed:
      DCHECK(mojom_icon_value->compressed.has_value());
      // For a compressed icon, the uncompressed image might be used to apply
      // icon effects, so both `uncompressed` and `compressed` need to be
      // copied.
      iv->uncompressed = mojom_icon_value->uncompressed;
      iv->compressed = std::move(mojom_icon_value->compressed.value());
      break;
    case mojom::IconType::kUncompressed:
    case mojom::IconType::kStandard:
      iv->uncompressed = mojom_icon_value->uncompressed;
      break;
  }

  return iv;
}

}  // namespace apps
