// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_MOJOM_TYPES_MOJOM_TRAITS_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_MOJOM_TYPES_MOJOM_TRAITS_H_

#include "ash/public/cpp/screen_backlight.h"
#include "chromeos/components/eche_app_ui/mojom/eche_app.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<chromeos::eche_app::mojom::ScreenBacklightState,
                  ash::ScreenBacklightState> {
  static chromeos::eche_app::mojom::ScreenBacklightState ToMojom(
      ash::ScreenBacklightState input);
  static bool FromMojom(chromeos::eche_app::mojom::ScreenBacklightState input,
                        ash::ScreenBacklightState* output);
};

}  // namespace mojo

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_MOJOM_TYPES_MOJOM_TRAITS_H_
