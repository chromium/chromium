// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_FEDERATED_PUBLIC_CPP_FEDERATED_EXAMPLE_UTIL_H_
#define CHROMEOS_ASH_SERVICES_FEDERATED_PUBLIC_CPP_FEDERATED_EXAMPLE_UTIL_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/services/federated/public/mojom/example.mojom.h"

namespace ash {
namespace federated {

// Helper functions for creating different ValueList.
COMPONENT_EXPORT(CHROMEOS_FEDERATED)
chromeos::federated::mojom::ValueListPtr CreateInt64List(
    const std::vector<int64_t>& values);
COMPONENT_EXPORT(CHROMEOS_FEDERATED)
chromeos::federated::mojom::ValueListPtr CreateFloatList(
    const std::vector<double>& values);
COMPONENT_EXPORT(CHROMEOS_FEDERATED)
chromeos::federated::mojom::ValueListPtr CreateStringList(
    const std::vector<std::string>& values);

}  // namespace federated
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_FEDERATED_PUBLIC_CPP_FEDERATED_EXAMPLE_UTIL_H_
