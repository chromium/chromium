// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/web_app_icon_types.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/strings/to_string.h"
#include "ui/gfx/skia_util.h"

namespace web_app {

namespace {

bool BitmapMapsEqual(const std::map<SquareSizePx, SkBitmap>& left,
                     const std::map<SquareSizePx, SkBitmap>& right) {
  return std::ranges::equal(left, right, [](const auto& lhs, const auto& rhs) {
    return lhs.first == rhs.first &&
           gfx::BitmapsAreEqual(lhs.second, rhs.second);
  });
}

}  // namespace

apps::IconInfo::Purpose ManifestPurposeToIconInfoPurpose(
    IconPurpose manifest_purpose) {
  switch (manifest_purpose) {
    case IconPurpose::ANY:
      return apps::IconInfo::Purpose::kAny;
    case IconPurpose::MONOCHROME:
      return apps::IconInfo::Purpose::kMonochrome;
    case IconPurpose::MASKABLE:
      return apps::IconInfo::Purpose::kMaskable;
  }
}

// IconBitmaps.
IconBitmaps::IconBitmaps() = default;
IconBitmaps::IconBitmaps(const IconBitmaps&) = default;
IconBitmaps::IconBitmaps(IconBitmaps&&) noexcept = default;
IconBitmaps& IconBitmaps::operator=(const IconBitmaps&) = default;
IconBitmaps& IconBitmaps::operator=(IconBitmaps&&) noexcept = default;
IconBitmaps::~IconBitmaps() = default;

bool IconBitmaps::operator==(const IconBitmaps& other) const {
  return BitmapMapsEqual(any, other.any) &&
         BitmapMapsEqual(maskable, other.maskable) &&
         BitmapMapsEqual(monochrome, other.monochrome);
}

const std::map<SquareSizePx, SkBitmap>& IconBitmaps::GetBitmapsForPurpose(
    IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::MONOCHROME:
      return monochrome;
    case IconPurpose::ANY:
      return any;
    case IconPurpose::MASKABLE:
      return maskable;
  }
}

void IconBitmaps::SetBitmapsForPurpose(
    IconPurpose purpose,
    std::map<SquareSizePx, SkBitmap> bitmaps) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(bitmaps);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(bitmaps);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(bitmaps);
      return;
  }
}

bool IconBitmaps::empty() const {
  return any.empty() && maskable.empty() && monochrome.empty();
}

// IconSizes.
IconSizes::IconSizes() = default;
IconSizes::IconSizes(const IconSizes&) = default;
IconSizes::IconSizes(IconSizes&&) noexcept = default;
IconSizes& IconSizes::operator=(const IconSizes&) = default;
IconSizes& IconSizes::operator=(IconSizes&&) noexcept = default;
IconSizes::~IconSizes() = default;

base::Value IconSizes::AsDebugValue() const {
  auto ConvertList = [](const auto& list) {
    base::ListValue list_json;
    for (const auto& item : list) {
      list_json.Append(item);
    }
    return list_json;
  };

  base::DictValue root;
  for (IconPurpose purpose : kIconPurposes) {
    root.Set(base::ToString(purpose), ConvertList(GetSizesForPurpose(purpose)));
  }

  return base::Value(std::move(root));
}

const std::vector<SquareSizePx>& IconSizes::GetSizesForPurpose(
    IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::MONOCHROME:
      return monochrome;
    case IconPurpose::ANY:
      return any;
    case IconPurpose::MASKABLE:
      return maskable;
  }
}

void IconSizes::SetSizesForPurpose(IconPurpose purpose,
                                   std::vector<SquareSizePx> sizes) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(sizes);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(sizes);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(sizes);
      return;
  }
}

bool IconSizes::empty() const {
  return any.empty() && maskable.empty() && monochrome.empty();
}

bool operator==(const IconSizes& icon_sizes1, const IconSizes& icon_sizes2) {
  return std::tie(icon_sizes1.any, icon_sizes1.maskable,
                  icon_sizes1.monochrome) == std::tie(icon_sizes2.any,
                                                      icon_sizes2.maskable,
                                                      icon_sizes2.monochrome);
}

}  // namespace web_app
