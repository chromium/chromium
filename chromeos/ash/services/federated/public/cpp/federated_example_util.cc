// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/federated/public/cpp/federated_example_util.h"

namespace ash {
namespace federated {

chromeos::federated::mojom::ValueListPtr CreateInt64List(
    const std::vector<int64_t>& values) {
  return chromeos::federated::mojom::ValueList::NewInt64List(
      chromeos::federated::mojom::Int64List::New(values));
}

chromeos::federated::mojom::ValueListPtr CreateFloatList(
    const std::vector<double>& values) {
  return chromeos::federated::mojom::ValueList::NewFloatList(
      chromeos::federated::mojom::FloatList::New(values));
}

chromeos::federated::mojom::ValueListPtr CreateStringList(
    const std::vector<std::string>& values) {
  return chromeos::federated::mojom::ValueList::NewStringList(
      chromeos::federated::mojom::StringList::New(values));
}

}  // namespace federated
}  // namespace ash
