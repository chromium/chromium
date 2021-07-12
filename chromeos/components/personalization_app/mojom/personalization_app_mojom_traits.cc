// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/personalization_app/mojom/personalization_app_mojom_traits.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

using MojomWallpaperLayout =
    chromeos::personalization_app::mojom::WallpaperLayout;
using MojomWallpaperType = chromeos::personalization_app::mojom::WallpaperType;

MojomWallpaperLayout
EnumTraits<MojomWallpaperLayout, ash::WallpaperLayout>::ToMojom(
    ash::WallpaperLayout input) {
  switch (input) {
    case ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER:
      return MojomWallpaperLayout::kCenter;
    case ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED:
      return MojomWallpaperLayout::kCenterCropped;
    case ash::WallpaperLayout::WALLPAPER_LAYOUT_STRETCH:
      return MojomWallpaperLayout::kStretch;
    case ash::WallpaperLayout::WALLPAPER_LAYOUT_TILE:
      return MojomWallpaperLayout::kTile;
    case ash::WallpaperLayout::NUM_WALLPAPER_LAYOUT:
      NOTREACHED();
      return MojomWallpaperLayout::kCenter;
  }
}

bool EnumTraits<MojomWallpaperLayout, ash::WallpaperLayout>::FromMojom(
    MojomWallpaperLayout input,
    ash::WallpaperLayout* output) {
  switch (input) {
    case MojomWallpaperLayout::kCenter:
      *output = ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER;
      return true;
    case MojomWallpaperLayout::kCenterCropped:
      *output = ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED;
      return true;
    case MojomWallpaperLayout::kStretch:
      *output = ash::WallpaperLayout::WALLPAPER_LAYOUT_STRETCH;
      return true;
    case MojomWallpaperLayout::kTile:
      *output = ash::WallpaperLayout::WALLPAPER_LAYOUT_TILE;
      return true;
  }
  NOTREACHED();
  return false;
}

MojomWallpaperType EnumTraits<MojomWallpaperType, ash::WallpaperType>::ToMojom(
    ash::WallpaperType input) {
  switch (input) {
    case ash::WallpaperType::DAILY:
      return MojomWallpaperType::kDaily;
    case ash::WallpaperType::CUSTOMIZED:
      return MojomWallpaperType::kCustomized;
    case ash::WallpaperType::DEFAULT:
      return MojomWallpaperType::kDefault;
    case ash::WallpaperType::ONLINE:
      return MojomWallpaperType::kOnline;
    case ash::WallpaperType::POLICY:
      return MojomWallpaperType::kPolicy;
    case ash::WallpaperType::THIRDPARTY:
      return MojomWallpaperType::kThirdParty;
    case ash::WallpaperType::DEVICE:
      return MojomWallpaperType::kDevice;
    case ash::WallpaperType::ONE_SHOT:
      return MojomWallpaperType::kOneShot;
    case ash::WallpaperType::WALLPAPER_TYPE_COUNT:
      NOTREACHED();
      return MojomWallpaperType::kDefault;
  }
}

bool EnumTraits<MojomWallpaperType, ash::WallpaperType>::FromMojom(
    MojomWallpaperType input,
    ash::WallpaperType* output) {
  switch (input) {
    case MojomWallpaperType::kDaily:
      *output = ash::WallpaperType::DAILY;
      return true;
    case MojomWallpaperType::kCustomized:
      *output = ash::WallpaperType::CUSTOMIZED;
      return true;
    case MojomWallpaperType::kDefault:
      *output = ash::WallpaperType::DEFAULT;
      return true;
    case MojomWallpaperType::kOnline:
      *output = ash::WallpaperType::ONLINE;
      return true;
    case MojomWallpaperType::kPolicy:
      *output = ash::WallpaperType::POLICY;
      return true;
    case MojomWallpaperType::kThirdParty:
      *output = ash::WallpaperType::THIRDPARTY;
      return true;
    case MojomWallpaperType::kDevice:
      *output = ash::WallpaperType::DEVICE;
      return true;
    case MojomWallpaperType::kOneShot:
      *output = ash::WallpaperType::ONE_SHOT;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
