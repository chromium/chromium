// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-shared.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(DRIVEFS_MOJOM)
    EnumTraits<drivefs::mojom::FileError, drive::FileError> {
  static drivefs::mojom::FileError ToMojom(drive::FileError input);

  static bool FromMojom(drivefs::mojom::FileError input,
                        drive::FileError* output);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_MOJOM_TRAITS_H_
