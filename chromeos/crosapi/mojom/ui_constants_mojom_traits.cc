// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/ui_constants_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

crosapi::mojom::ResourceScaleFactor
EnumTraits<crosapi::mojom::ResourceScaleFactor,
           ui::ResourceScaleFactor>::ToMojom(ui::ResourceScaleFactor input) {
  switch (input) {
    case ui::ResourceScaleFactor::kScaleFactorNone:
      return crosapi::mojom::ResourceScaleFactor::kScaleFactorNone;
    case ui::ResourceScaleFactor::k100Percent:
      return crosapi::mojom::ResourceScaleFactor::k100Percent;
    case ui::ResourceScaleFactor::k200Percent:
      return crosapi::mojom::ResourceScaleFactor::k200Percent;
    case ui::ResourceScaleFactor::k300Percent:
      return crosapi::mojom::ResourceScaleFactor::k300Percent;
    case ui::ResourceScaleFactor::NUM_SCALE_FACTORS:
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::ResourceScaleFactor::kScaleFactorNone;
  }
}

bool EnumTraits<crosapi::mojom::ResourceScaleFactor, ui::ResourceScaleFactor>::
    FromMojom(crosapi::mojom::ResourceScaleFactor input,
              ui::ResourceScaleFactor* output) {
  switch (input) {
    case crosapi::mojom::ResourceScaleFactor::kScaleFactorNone:
      *output = ui::ResourceScaleFactor::kScaleFactorNone;
      return true;
    case crosapi::mojom::ResourceScaleFactor::k100Percent:
      *output = ui::ResourceScaleFactor::k100Percent;
      return true;
    case crosapi::mojom::ResourceScaleFactor::k200Percent:
      *output = ui::ResourceScaleFactor::k200Percent;
      return true;
    case crosapi::mojom::ResourceScaleFactor::k300Percent:
      *output = ui::ResourceScaleFactor::k300Percent;
      return true;
  }
}

}  // namespace mojo
