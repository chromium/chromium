// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_suggest_test_util.h"

#include "base/json/json_writer.h"

namespace app_list {

std::string CreateItemSuggestUpdateJsonString(
    const std::vector<SuggestItemMetadata>& item_data_array,
    const std::string& session_id) {
  base::Value::List list_value;
  for (const auto& data : item_data_array) {
    base::Value::Dict dict_value;
    dict_value.Set("itemId", data.item_id);
    dict_value.Set("displayText", data.display_text);
    dict_value.Set("predictionReason", data.prediction_reason);
    list_value.Append(std::move(dict_value));
  }

  base::Value::Dict suggest_item_update;
  suggest_item_update.Set("item", std::move(list_value));
  suggest_item_update.Set("suggestionSessionId", session_id);

  std::string json_string;
  base::JSONWriter::Write(suggest_item_update, &json_string);
  return json_string;
}

}  // namespace app_list
