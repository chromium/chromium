// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_UI_CONSTANTS_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_UI_CONSTANTS_MOJOM_TRAITS_H_

#include "chromeos/crosapi/mojom/ui_constants.mojom.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace mojo {

template <>
struct EnumTraits<crosapi::mojom::ResourceScaleFactor,
                  ui::ResourceScaleFactor> {
  static crosapi::mojom::ResourceScaleFactor ToMojom(
      ui::ResourceScaleFactor input);
  static bool FromMojom(crosapi::mojom::ResourceScaleFactor input,
                        ui::ResourceScaleFactor* output);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_UI_CONSTANTS_MOJOM_TRAITS_H_
