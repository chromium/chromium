// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_FEDERATED_PUBLIC_CPP_FEDERATED_EXAMPLE_UTIL_H_
#define CHROMEOS_SERVICES_FEDERATED_PUBLIC_CPP_FEDERATED_EXAMPLE_UTIL_H_

#include <string>
#include <vector>

#include "chromeos/services/federated/public/mojom/example.mojom.h"

namespace chromeos {
namespace federated {

// Helper functions for creating different ValueList.
mojom::ValueListPtr CreateInt64List(const std::vector<int64_t>& values);
mojom::ValueListPtr CreateFloatList(const std::vector<double>& values);
mojom::ValueListPtr CreateStringList(const std::vector<std::string>& values);

}  // namespace federated
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_FEDERATED_PUBLIC_CPP_FEDERATED_EXAMPLE_UTIL_H_
