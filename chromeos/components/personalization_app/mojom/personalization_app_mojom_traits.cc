// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/personalization_app/mojom/personalization_app_mojom_traits.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "chromeos/components/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/mojom/url_gurl_mojom_traits.h"

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

const std::string&
StructTraits<chromeos::personalization_app::mojom::WallpaperCollectionDataView,
             backdrop::Collection>::id(const backdrop::Collection& collection) {
  return collection.collection_id();
}

const std::string& StructTraits<
    chromeos::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection>::name(const backdrop::Collection& collection) {
  return collection.collection_name();
}

absl::optional<GURL> StructTraits<
    chromeos::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection>::preview(const backdrop::Collection& collection) {
  return GURL(collection.preview(0).image_url());
}

// Default to false as we don't ever need to convert back to
// |backdrop::Collection|
bool StructTraits<
    chromeos::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection>::
    Read(chromeos::personalization_app::mojom::WallpaperCollectionDataView data,
         backdrop::Collection* out) {
  return false;
}

bool StructTraits<
    chromeos::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection>::isNull(const backdrop::Collection& collection) {
  return !(collection.has_collection_id() && collection.has_collection_name() &&
           collection.preview_size() > 0);
}

GURL StructTraits<chromeos::personalization_app::mojom::WallpaperImageDataView,
                  backdrop::Image>::url(const backdrop::Image& image) {
  return GURL(image.image_url());
}

std::vector<std::string>
StructTraits<chromeos::personalization_app::mojom::WallpaperImageDataView,
             backdrop::Image>::attribution(const backdrop::Image& image) {
  std::vector<std::string> attribution;
  for (const auto& attr : image.attribution())
    attribution.push_back(attr.text());
  return attribution;
}

uint64_t
StructTraits<chromeos::personalization_app::mojom::WallpaperImageDataView,
             backdrop::Image>::asset_id(const backdrop::Image& image) {
  return image.asset_id();
}

// Default to false as we don't ever need to convert back to
// |backdrop::Image|
bool StructTraits<chromeos::personalization_app::mojom::WallpaperImageDataView,
                  backdrop::Image>::
    Read(chromeos::personalization_app::mojom::WallpaperImageDataView data,
         backdrop::Image* out) {
  return false;
}

bool StructTraits<chromeos::personalization_app::mojom::WallpaperImageDataView,
                  backdrop::Image>::isNull(const backdrop::Image& image) {
  if (!image.has_image_url() || !image.has_asset_id())
    return true;
  GURL image_url(image.image_url());
  if (!image_url.is_valid())
    return true;
  return false;
}

base::UnguessableToken StructTraits<
    chromeos::personalization_app::mojom::LocalImageDataView,
    ash::LocalImageInfo>::id(const ash::LocalImageInfo& local_image_info) {
  return local_image_info.id;
}

std::string StructTraits<
    chromeos::personalization_app::mojom::LocalImageDataView,
    ash::LocalImageInfo>::name(const ash::LocalImageInfo& local_image_info) {
  return local_image_info.path.BaseName().value();
}

// Default to false as we don't ever need to convert back to
// |ash::LocalImageInfo|
bool StructTraits<chromeos::personalization_app::mojom::LocalImageDataView,
                  ash::LocalImageInfo>::
    Read(chromeos::personalization_app::mojom::LocalImageDataView data,
         ash::LocalImageInfo* out) {
  return false;
}

}  // namespace mojo
