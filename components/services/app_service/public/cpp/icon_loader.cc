// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/icon_loader.h"

#include <utility>

#include "base/functional/callback.h"

namespace apps {

IconLoader::Releaser::Releaser(std::unique_ptr<IconLoader::Releaser> next,
                               base::OnceClosure closure)
    : next_(std::move(next)), closure_(std::move(closure)) {}

IconLoader::Releaser::~Releaser() {
  std::move(closure_).Run();
}

IconLoader::Key::Key(const std::string& id,
                     const IconKey& icon_key,
                     IconType icon_type,
                     int32_t size_hint_in_dip,
                     bool allow_placeholder_icon)
    : id_(id),
      timeline_(absl::holds_alternative<int32_t>(icon_key.update_version)
                    ? absl::get<int32_t>(icon_key.update_version)
                    : IconKey::kInvalidVersion),
      resource_id_(icon_key.resource_id),
      icon_effects_(icon_key.icon_effects),
      icon_type_(icon_type),
      size_hint_in_dip_(size_hint_in_dip),
      allow_placeholder_icon_(allow_placeholder_icon) {}

IconLoader::Key::Key(const Key& other) = default;

bool IconLoader::Key::operator<(const Key& that) const {
  if (this->timeline_ != that.timeline_) {
    return this->timeline_ < that.timeline_;
  }
  if (this->resource_id_ != that.resource_id_) {
    return this->resource_id_ < that.resource_id_;
  }
  if (this->icon_effects_ != that.icon_effects_) {
    return this->icon_effects_ < that.icon_effects_;
  }
  if (this->icon_type_ != that.icon_type_) {
    return this->icon_type_ < that.icon_type_;
  }
  if (this->size_hint_in_dip_ != that.size_hint_in_dip_) {
    return this->size_hint_in_dip_ < that.size_hint_in_dip_;
  }
  if (this->allow_placeholder_icon_ != that.allow_placeholder_icon_) {
    return this->allow_placeholder_icon_ < that.allow_placeholder_icon_;
  }
  return this->id_ < that.id_;
}

IconLoader::IconLoader() = default;

IconLoader::~IconLoader() = default;

std::optional<IconKey> IconLoader::GetIconKey(const std::string& id) {
  return std::make_optional<IconKey>();
}

std::unique_ptr<IconLoader::Releaser> IconLoader::LoadIcon(
    const std::string& id,
    const IconType& icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  auto icon_key = GetIconKey(id);
  if (!icon_key.has_value()) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return nullptr;
  }

  return LoadIconFromIconKey(id, icon_key.value(), icon_type, size_hint_in_dip,
                             allow_placeholder_icon, std::move(callback));
}

}  // namespace apps
