// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/parsing_utils.h"

#include "base/json/json_reader.h"
#include "base/values.h"

namespace record_replay {

std::vector<base::Value> ParseJSONListOfDicts(std::string_view json_string) {
  std::vector<base::Value> dicts;
  auto value = base::JSONReader::Read(json_string,
                                      base::JSONParserOptions::JSON_PARSE_RFC);
  if (!value || !value->is_list()) {
    return dicts;
  }

  base::ListValue list = std::move(*value).TakeList();
  for (base::Value& item : list) {
    if (item.is_dict()) {
      dicts.push_back(std::move(item));
    }
  }
  return dicts;
}

}  // namespace record_replay
