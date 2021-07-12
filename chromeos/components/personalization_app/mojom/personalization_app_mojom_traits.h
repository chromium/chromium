// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_
#define CHROMEOS_COMPONENTS_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

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

}  // namespace mojo

#endif  // CHROMEOS_COMPONENTS_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_
