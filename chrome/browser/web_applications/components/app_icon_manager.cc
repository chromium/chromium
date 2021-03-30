// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_icon_manager.h"

#include <map>
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

namespace {

void WrapReadIconCallback(AppIconManager::ReadIconCallback callback,
                          IconPurpose ignored,
                          const SkBitmap& bitmap) {
  std::move(callback).Run(bitmap);
}

void WrapReadCompressedIconCallback(
    AppIconManager::ReadCompressedIconCallback callback,
    IconPurpose ignored,
    std::vector<uint8_t> data) {
  std::move(callback).Run(std::move(data));
}

}  // namespace

void AppIconManager::ReadSmallestIconAny(const AppId& app_id,
                                         SquareSizePx min_icon_size,
                                         ReadIconCallback callback) const {
  ReadIconWithPurposeCallback wrapped =
      base::BindOnce(WrapReadIconCallback, std::move(callback));
  ReadSmallestIcon(app_id, {IconPurpose::ANY}, min_icon_size,
                   std::move(wrapped));
}

void AppIconManager::ReadSmallestCompressedIconAny(
    const AppId& app_id,
    SquareSizePx min_icon_size,
    ReadCompressedIconCallback callback) const {
  ReadCompressedIconWithPurposeCallback wrapped =
      base::BindOnce(WrapReadCompressedIconCallback, std::move(callback));
  ReadSmallestCompressedIcon(app_id, {IconPurpose::ANY}, min_icon_size,
                             std::move(wrapped));
}

// static
void AppIconManager::WrapReadIconWithPurposeCallback(
    ReadIconWithPurposeCallback callback,
    IconPurpose purpose,
    const SkBitmap& bitmap) {
  std::move(callback).Run(purpose, bitmap);
}

}  // namespace web_app
