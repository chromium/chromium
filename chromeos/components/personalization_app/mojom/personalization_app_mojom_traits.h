// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_
#define CHROMEOS_COMPONENTS_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/unguessable_token.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "chromeos/components/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace mojo {

template <>
struct EnumTraits<chromeos::personalization_app::mojom::WallpaperLayout,
                  ash::WallpaperLayout> {
  using MojomWallpaperLayout =
      ::chromeos::personalization_app::mojom::WallpaperLayout;
  static MojomWallpaperLayout ToMojom(ash::WallpaperLayout input);
  static bool FromMojom(MojomWallpaperLayout input,
                        ash::WallpaperLayout* output);
};

template <>
struct EnumTraits<chromeos::personalization_app::mojom::WallpaperType,
                  ash::WallpaperType> {
  using MojomWallpaperType =
      ::chromeos::personalization_app::mojom::WallpaperType;
  static MojomWallpaperType ToMojom(ash::WallpaperType input);
  static bool FromMojom(MojomWallpaperType input, ash::WallpaperType* output);
};

template <>
struct StructTraits<
    chromeos::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection> {
  static const std::string& id(const backdrop::Collection& collection);
  static const std::string& name(const backdrop::Collection& collection);
  static absl::optional<GURL> preview(const backdrop::Collection& collection);

  static bool Read(
      chromeos::personalization_app::mojom::WallpaperCollectionDataView data,
      backdrop::Collection* out);
  static bool isNull(const backdrop::Collection& collection);
};

template <>
struct StructTraits<
    chromeos::personalization_app::mojom::WallpaperImageDataView,
    backdrop::Image> {
  static GURL url(const backdrop::Image& image);
  static std::vector<std::string> attribution(const backdrop::Image& image);
  static uint64_t asset_id(const backdrop::Image& image);

  static bool Read(
      chromeos::personalization_app::mojom::WallpaperImageDataView data,
      backdrop::Image* out);
  static bool isNull(const backdrop::Image& image);
};

}  // namespace mojo

#endif  // CHROMEOS_COMPONENTS_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_
