// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/dialog_image_info.h"

#include <algorithm>

#include "ui/gfx/skia_util.h"

namespace web_app {
namespace {

bool BitmapMapsEqual(const std::map<int, SkBitmap>& left,
                     const std::map<int, SkBitmap>& right) {
  return std::ranges::equal(left, right, [](const auto& lhs, const auto& rhs) {
    return lhs.first == rhs.first &&
           gfx::BitmapsAreEqual(lhs.second, rhs.second);
  });
}

}  // namespace

DialogImageInfo::DialogImageInfo() = default;
DialogImageInfo::~DialogImageInfo() = default;
DialogImageInfo::DialogImageInfo(const DialogImageInfo& dialog_image_info) =
    default;
DialogImageInfo& DialogImageInfo::operator=(
    const DialogImageInfo& dialog_image_info) = default;
DialogImageInfo::DialogImageInfo(DialogImageInfo&& dialog_image_info) = default;
DialogImageInfo& DialogImageInfo::operator=(
    DialogImageInfo&& dialog_image_info) = default;

bool operator==(const DialogImageInfo& info1, const DialogImageInfo& info2) {
  return BitmapMapsEqual(info1.bitmaps, info2.bitmaps) &&
         info1.is_maskable == info2.is_maskable;
}

}  // namespace web_app
