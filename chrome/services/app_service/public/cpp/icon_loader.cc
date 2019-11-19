// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/icon_loader.h"

#include <utility>

#include "base/callback.h"

namespace apps {

IconLoader::Releaser::Releaser(std::unique_ptr<IconLoader::Releaser> next,
                               base::OnceClosure closure)
    : next_(std::move(next)), closure_(std::move(closure)) {}

IconLoader::Releaser::~Releaser() {
  std::move(closure_).Run();
}

IconLoader::Key::Key(apps::mojom::AppType app_type,
                     const std::string& app_id,
                     const apps::mojom::IconKeyPtr& icon_key,
                     apps::mojom::IconCompression icon_compression,
                     int32_t size_hint_in_dip,
                     bool allow_placeholder_icon)
    : app_type_(app_type),
      app_id_(app_id),
      timeline_(icon_key ? icon_key->timeline : 0),
      resource_id_(icon_key ? icon_key->resource_id : 0),
      icon_effects_(icon_key ? icon_key->icon_effects : 0),
      icon_compression_(icon_compression),
      size_hint_in_dip_(size_hint_in_dip),
      allow_placeholder_icon_(allow_placeholder_icon) {}

IconLoader::Key::Key(const Key& other) = default;

bool IconLoader::Key::operator<(const Key& that) const {
  if (this->app_type_ != that.app_type_) {
    return this->app_type_ < that.app_type_;
  }
  if (this->timeline_ != that.timeline_) {
    return this->timeline_ < that.timeline_;
  }
  if (this->resource_id_ != that.resource_id_) {
    return this->resource_id_ < that.resource_id_;
  }
  if (this->icon_effects_ != that.icon_effects_) {
    return this->icon_effects_ < that.icon_effects_;
  }
  if (this->icon_compression_ != that.icon_compression_) {
    return this->icon_compression_ < that.icon_compression_;
  }
  if (this->size_hint_in_dip_ != that.size_hint_in_dip_) {
    return this->size_hint_in_dip_ < that.size_hint_in_dip_;
  }
  if (this->allow_placeholder_icon_ != that.allow_placeholder_icon_) {
    return this->allow_placeholder_icon_ < that.allow_placeholder_icon_;
  }
  return this->app_id_ < that.app_id_;
}

IconLoader::IconLoader() = default;

IconLoader::~IconLoader() = default;

std::unique_ptr<IconLoader::Releaser> IconLoader::LoadIcon(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  return LoadIconFromIconKey(app_type, app_id, GetIconKey(app_id),
                             icon_compression, size_hint_in_dip,
                             allow_placeholder_icon, std::move(callback));
}

}  // namespace apps
