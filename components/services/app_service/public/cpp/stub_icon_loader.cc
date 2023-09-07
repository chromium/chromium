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

absl::optional<IconKey> StubIconLoader::GetIconKey(const std::string& id) {
  uint64_t timeline = 0;
  auto iter = timelines_by_app_id_.find(id);
  if (iter != timelines_by_app_id_.end()) {
    timeline = iter->second;
  }
  return absl::make_optional<IconKey>(timeline, 0, 0);
}

std::unique_ptr<IconLoader::Releaser> StubIconLoader::LoadIconFromIconKey(
    const std::string& id,
    const IconKey& icon_key,
    IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  num_load_calls_++;
  if (base::Contains(timelines_by_app_id_, id)) {
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
