// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/mojom/types_mojom_traits.h"

namespace mojo {

// static
chromeos::eche_app::mojom::ScreenBacklightState
EnumTraits<chromeos::eche_app::mojom::ScreenBacklightState,
           ash::ScreenState>::ToMojom(ash::ScreenState input) {
  switch (input) {
    case ash::ScreenState::ON:
      return chromeos::eche_app::mojom::ScreenBacklightState::ON;
    case ash::ScreenState::OFF:
      return chromeos::eche_app::mojom::ScreenBacklightState::OFF;
    case ash::ScreenState::OFF_AUTO:
      return chromeos::eche_app::mojom::ScreenBacklightState::OFF_AUTO;
  }

  NOTREACHED();
}

// static
bool EnumTraits<chromeos::eche_app::mojom::ScreenBacklightState,
                ash::ScreenState>::
    FromMojom(chromeos::eche_app::mojom::ScreenBacklightState input,
              ash::ScreenState* output) {
  switch (input) {
    case chromeos::eche_app::mojom::ScreenBacklightState::ON:
      *output = ash::ScreenState::ON;
      return true;
    case chromeos::eche_app::mojom::ScreenBacklightState::OFF:
      *output = ash::ScreenState::OFF;
      return true;
    case chromeos::eche_app::mojom::ScreenBacklightState::OFF_AUTO:
      *output = ash::ScreenState::OFF_AUTO;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
