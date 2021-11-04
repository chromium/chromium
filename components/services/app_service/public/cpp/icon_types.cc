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

bool IconKey::operator==(const IconKey& other) const {
  return timeline == other.timeline && resource_id == other.resource_id &&
         icon_effects == other.icon_effects;
}

constexpr uint64_t IconKey::kDoesNotChangeOverTime = 0;
constexpr int32_t IconKey::kInvalidResourceId = 0;

IconValue::IconValue() = default;
IconValue::~IconValue() = default;

std::unique_ptr<IconKey> ConvertMojomIconKeyToIconKey(
    apps::mojom::IconKeyPtr mojom_icon_key) {
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
      iv->compressed = std::move(mojom_icon_value->compressed.value());
      break;
    case mojom::IconType::kUncompressed:
    case mojom::IconType::kStandard:
      iv->uncompressed = mojom_icon_value->uncompressed;
      break;
  }

  return iv;
}

base::OnceCallback<void(std::unique_ptr<IconValue>)>
IconValueToMojomIconValueCallback(
    base::OnceCallback<void(apps::mojom::IconValuePtr)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(apps::mojom::IconValuePtr)> inner_callback,
         std::unique_ptr<IconValue> icon_value) {
        std::move(inner_callback)
            .Run(ConvertIconValueToMojomIconValue(std::move(icon_value)));
      },
      std::move(callback));
}
}  // namespace apps
