// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/icon_types.h"

namespace apps {

IconKey::IconKey() = default;

IconKey::IconKey(uint32_t icon_effects) : icon_effects(icon_effects) {}

IconKey::IconKey(int32_t resource_id, uint32_t icon_effects)
    : resource_id(resource_id), icon_effects(icon_effects) {}

IconKey::IconKey(bool raw_icon_updated, uint32_t icon_effects)
    : update_version(raw_icon_updated), icon_effects(icon_effects) {}

IconKey::~IconKey() = default;

bool IconKey::operator==(const IconKey& other) const {
  return update_version == other.update_version &&
         resource_id == other.resource_id && icon_effects == other.icon_effects;
}

bool IconKey::operator!=(const IconKey& other) const {
  return !(*this == other);
}

IconKeyPtr IconKey::Clone() const {
  auto icon_key = std::make_unique<IconKey>(resource_id, icon_effects);
  icon_key->update_version = update_version;
  return icon_key;
}

bool IconKey::HasUpdatedVersion() const {
  return absl::holds_alternative<bool>(update_version) &&
         absl::get<bool>(update_version);
}

constexpr int32_t IconKey::kInvalidResourceId = 0;
constexpr int32_t IconKey::kInitVersion = 0;
constexpr int32_t IconKey::kInvalidVersion = -1;

IconValue::IconValue() = default;
IconValue::~IconValue() = default;

std::optional<apps::IconKey> MergeIconKey(const apps::IconKey* state,
                                          const apps::IconKey* delta) {
  //`state` should have int32_t `update_version` only.
  CHECK(!state || absl::holds_alternative<int32_t>(state->update_version));

  // `delta` should hold a bool icon version only.
  CHECK(!delta || absl::holds_alternative<bool>(delta->update_version));

  if (!delta) {
    if (state) {
      return std::move(*state->Clone());
    }
    return std::nullopt;
  }

  IconKey icon_key = IconKey(delta->resource_id, delta->icon_effects);

  if (delta->resource_id != IconKey::kInvalidResourceId) {
    icon_key.update_version = IconKey::kInvalidVersion;
    return icon_key;
  }

  if (!state) {
    icon_key.update_version = IconKey::kInitVersion;
    return icon_key;
  }

  icon_key.update_version = absl::get<int32_t>(state->update_version);

  // The icon is updated by the app, so increase `update_version`.
  if (delta->HasUpdatedVersion()) {
    ++absl::get<int32_t>(icon_key.update_version);
  }
  return icon_key;
}

}  // namespace apps
