// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/federated/public/cpp/federated_example_util.h"

namespace chromeos {
namespace federated {

mojom::ValueListPtr CreateInt64List(const std::vector<int64_t>& values) {
  mojom::ValueListPtr value_list = mojom::ValueList::New();
  value_list->set_int64_list(mojom::Int64List::New());
  value_list->get_int64_list()->value = values;
  return value_list;
}

mojom::ValueListPtr CreateFloatList(const std::vector<double>& values) {
  mojom::ValueListPtr value_list = mojom::ValueList::New();
  value_list->set_float_list(mojom::FloatList::New());
  value_list->get_float_list()->value = values;
  return value_list;
}

mojom::ValueListPtr CreateStringList(const std::vector<std::string>& values) {
  mojom::ValueListPtr value_list = mojom::ValueList::New();
  value_list->set_string_list(mojom::StringList::New());
  value_list->get_string_list()->value = values;
  return value_list;
}

}  // namespace federated
}  // namespace chromeos
