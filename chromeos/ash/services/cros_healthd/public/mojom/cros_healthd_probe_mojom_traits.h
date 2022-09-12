// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_MOJOM_CROS_HEALTHD_PROBE_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_MOJOM_CROS_HEALTHD_PROBE_MOJOM_TRAITS_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
class EnumTraits<ash::cros_healthd::mojom::CpuArchitectureEnum,
                 enterprise_management::CpuInfo::Architecture> {
 public:
  static ash::cros_healthd::mojom::CpuArchitectureEnum ToMojom(
      enterprise_management::CpuInfo::Architecture input);
  static bool FromMojom(ash::cros_healthd::mojom::CpuArchitectureEnum input,
                        enterprise_management::CpuInfo::Architecture* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_MOJOM_CROS_HEALTHD_PROBE_MOJOM_TRAITS_H_
