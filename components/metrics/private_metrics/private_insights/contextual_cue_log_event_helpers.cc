// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/contextual_cue_log_event_helpers.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace private_insights {

std::string SerializePageInfoListToJson(
    const std::vector<events::ContextualCueLogEvent::PageInfo>& list) {
  base::ListValue value_list;
  for (const auto& item : list) {
    base::DictValue dict;
    dict.Set("url", item.url());
    dict.Set("title", item.title());
    value_list.Append(std::move(dict));
  }
  std::string json_output;
  if (base::JSONWriter::Write(value_list, &json_output)) {
    return json_output;
  }
  return "[]";
}

}  // namespace private_insights
