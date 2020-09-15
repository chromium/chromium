// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_MOJOM_CROS_HEALTHD_PROBE_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_MOJOM_CROS_HEALTHD_PROBE_MOJOM_TRAITS_H_

#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
class EnumTraits<chromeos::cros_healthd::mojom::CpuArchitectureEnum,
                 enterprise_management::CpuInfo::Architecture> {
 public:
  static chromeos::cros_healthd::mojom::CpuArchitectureEnum ToMojom(
      enterprise_management::CpuInfo::Architecture input);
  static bool FromMojom(
      chromeos::cros_healthd::mojom::CpuArchitectureEnum input,
      enterprise_management::CpuInfo::Architecture* out);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_MOJOM_CROS_HEALTHD_PROBE_MOJOM_TRAITS_H_
