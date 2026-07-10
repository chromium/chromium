// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"

namespace private_insights {

// Serializes a collection of items to a JSON array string using an extractor
// function. UrlAndTitleExtractor should have signature:
// std::optional<std::pair<std::string, std::string>>(const T&) returning {url,
// title} or std::nullopt to skip the item.
template <typename T, typename UrlAndTitleExtractor>
std::string SerializeCollectionToPageInfoJson(const std::vector<T>& list,
                                              UrlAndTitleExtractor extractor) {
  base::ListValue value_list;
  for (const auto& item : list) {
    auto result = extractor(item);
    if (result.has_value()) {
      base::DictValue dict;
      dict.Set("url", std::move(result->first));
      dict.Set("title", std::move(result->second));
      value_list.Append(std::move(dict));
    }
  }
  std::string json_output;
  if (base::JSONWriter::Write(value_list, &json_output)) {
    return json_output;
  }
  return "[]";
}

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_
