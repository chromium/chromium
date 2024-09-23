// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe_mojom_traits.h"

#include "base/notreached.h"

namespace em = enterprise_management;

namespace mojo {

ash::cros_healthd::mojom::CpuArchitectureEnum EnumTraits<
    ash::cros_healthd::mojom::CpuArchitectureEnum,
    em::CpuInfo::Architecture>::ToMojom(em::CpuInfo::Architecture input) {
  switch (input) {
    case em::CpuInfo::ARCHITECTURE_UNSPECIFIED:
      return ash::cros_healthd::mojom::CpuArchitectureEnum::kUnknown;
    case em::CpuInfo::X86_64:
      return ash::cros_healthd::mojom::CpuArchitectureEnum::kX86_64;
    case em::CpuInfo::AARCH64:
      return ash::cros_healthd::mojom::CpuArchitectureEnum::kAArch64;
    case em::CpuInfo::ARMV7L:
      return ash::cros_healthd::mojom::CpuArchitectureEnum::kArmv7l;
  }

  NOTREACHED_IN_MIGRATION();
  return ash::cros_healthd::mojom::CpuArchitectureEnum::kUnknown;
}

bool EnumTraits<ash::cros_healthd::mojom::CpuArchitectureEnum,
                em::CpuInfo::Architecture>::
    FromMojom(ash::cros_healthd::mojom::CpuArchitectureEnum input,
              em::CpuInfo::Architecture* out) {
  switch (input) {
    case ash::cros_healthd::mojom::CpuArchitectureEnum::kUnknown:
      *out = em::CpuInfo::ARCHITECTURE_UNSPECIFIED;
      return true;
    case ash::cros_healthd::mojom::CpuArchitectureEnum::kX86_64:
      *out = em::CpuInfo::X86_64;
      return true;
    case ash::cros_healthd::mojom::CpuArchitectureEnum::kAArch64:
      *out = em::CpuInfo::AARCH64;
      return true;
    case ash::cros_healthd::mojom::CpuArchitectureEnum::kArmv7l:
      *out = em::CpuInfo::ARMV7L;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
