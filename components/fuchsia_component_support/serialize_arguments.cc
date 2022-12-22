// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/serialize_arguments.h"

#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"

namespace fuchsia_component_support {

std::vector<uint8_t> SerializeArguments(const base::CommandLine& command_line) {
  base::Value::List argv_list;
  const auto& argv = command_line.argv();
  DCHECK_GE(argv.size(), 1UL);
  argv_list.reserve(argv.size() - 1);
  for (size_t i = 1, size = argv.size(); i < size; ++i) {
    argv_list.Append(argv[i]);
  }

  base::Value::Dict feature_dict;
  feature_dict.Set("argv", std::move(argv_list));
  std::string json_string;
  CHECK(JSONStringValueSerializer(&json_string)
            .Serialize(base::Value(std::move(feature_dict))));
  return std::vector<uint8_t>(json_string.begin(), json_string.end());
}

}  // namespace fuchsia_component_support
