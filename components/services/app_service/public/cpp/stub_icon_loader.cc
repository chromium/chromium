// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/stub_icon_loader.h"

#include <utility>

#include "base/containers/contains.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace apps {

StubIconLoader::StubIconLoader() = default;

StubIconLoader::~StubIconLoader() = default;

std::optional<IconKey> StubIconLoader::GetIconKey(const std::string& id) {
  int32_t update_version = IconKey::kInitVersion;
  auto iter = update_version_by_app_id_.find(id);
  if (iter != update_version_by_app_id_.end()) {
    update_version = iter->second;
  }
  auto icon_key = std::make_optional<IconKey>();
  icon_key->update_version = update_version;
  return icon_key;
}

std::unique_ptr<IconLoader::Releaser> StubIconLoader::LoadIconFromIconKey(
    const std::string& id,
    const IconKey& icon_key,
    IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  num_load_calls_++;
  if (base::Contains(update_version_by_app_id_, id)) {
    auto icon_value = std::make_unique<IconValue>();
    icon_value->icon_type = icon_type;
    icon_value->uncompressed =
        gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
    std::move(callback).Run(std::move(icon_value));
  } else {
    std::move(callback).Run(std::make_unique<IconValue>());
  }
  return nullptr;
}

int StubIconLoader::NumLoadIconFromIconKeyCalls() {
  return num_load_calls_;
}

}  // namespace apps
