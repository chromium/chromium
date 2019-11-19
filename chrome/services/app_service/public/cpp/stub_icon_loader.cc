// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/stub_icon_loader.h"

#include <utility>

#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace apps {

StubIconLoader::StubIconLoader() = default;

StubIconLoader::~StubIconLoader() = default;

apps::mojom::IconKeyPtr StubIconLoader::GetIconKey(const std::string& app_id) {
  uint64_t timeline = 0;
  auto iter = timelines_by_app_id_.find(app_id);
  if (iter != timelines_by_app_id_.end()) {
    timeline = iter->second;
  }
  return apps::mojom::IconKey::New(timeline, 0, 0);
}

std::unique_ptr<IconLoader::Releaser> StubIconLoader::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  num_load_calls_++;
  auto iter = timelines_by_app_id_.find(app_id);
  if (iter != timelines_by_app_id_.end()) {
    auto icon_value = apps::mojom::IconValue::New();
    icon_value->icon_compression = apps::mojom::IconCompression::kUncompressed;
    icon_value->uncompressed =
        gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
    std::move(callback).Run(std::move(icon_value));
  } else {
    std::move(callback).Run(apps::mojom::IconValue::New());
  }
  return nullptr;
}

int StubIconLoader::NumLoadIconFromIconKeyCalls() {
  return num_load_calls_;
}

}  // namespace apps
