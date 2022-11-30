// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_SHARESHEET_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_SHARESHEET_MOJOM_TRAITS_H_

#include "chromeos/components/sharesheet/constants.h"
#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<crosapi::mojom::SharesheetLaunchSource,
                  sharesheet::LaunchSource> {
  static crosapi::mojom::SharesheetLaunchSource ToMojom(
      sharesheet::LaunchSource input);
  static bool FromMojom(crosapi::mojom::SharesheetLaunchSource input,
                        sharesheet::LaunchSource* output);
};

template <>
struct EnumTraits<crosapi::mojom::SharesheetResult,
                  sharesheet::SharesheetResult> {
  static crosapi::mojom::SharesheetResult ToMojom(
      sharesheet::SharesheetResult input);
  static bool FromMojom(crosapi::mojom::SharesheetResult input,
                        sharesheet::SharesheetResult* output);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_SHARESHEET_MOJOM_TRAITS_H_
