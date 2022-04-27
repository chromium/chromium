// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/federated/public/cpp/federated_example_util.h"

namespace chromeos {
namespace federated {

mojom::ValueListPtr CreateInt64List(const std::vector<int64_t>& values) {
  return mojom::ValueList::NewInt64List(mojom::Int64List::New(values));
}

mojom::ValueListPtr CreateFloatList(const std::vector<double>& values) {
  return mojom::ValueList::NewFloatList(mojom::FloatList::New(values));
}

mojom::ValueListPtr CreateStringList(const std::vector<std::string>& values) {
  return mojom::ValueList::NewStringList(mojom::StringList::New(values));
}

}  // namespace federated
}  // namespace chromeos
