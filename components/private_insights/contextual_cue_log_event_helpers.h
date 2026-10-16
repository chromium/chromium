// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_
#define COMPONENTS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/private_insights/events/coarse_system_profile.pb.h"
#include "components/private_insights/events/contextual_cue_log_event.pb.h"

class GURL;

namespace optimization_guide::proto {
class ContextualCue;
class Tab;
}  // namespace optimization_guide::proto

namespace private_insights {

// Platform-independent building blocks for `ContextualCueLogEvent`. Each
// platform adapter (desktop `chrome/browser/contextual_cueing`, iOS
// `ios/chrome/browser/intelligence/contextual_cueing`) composes these with the
// inputs only it can provide: the active tab, the channel, the locale and the
// country.

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

// Extractor for `SerializeCollectionToPageInfoJson` over the background tabs
// sent with a `ContextualCueingRequest`. Never skips an item: an empty tab
// serializes with empty `url` and `title`.
std::optional<std::pair<std::string, std::string>> ExtractFromProtoTab(
    const optimization_guide::proto::Tab& tab);

// Fills the fields of `system_profile` that do not depend on the embedder:
// `chrome_version`, `milestone` and `platform` (the UMA operating system
// name, e.g. "Windows NT" or "iOS"). Callers fill `chrome_channel`,
// `language` and `country`.
void PopulateCommonSystemProfile(events::CoarseSystemProfile* system_profile);

// Fills `active_page` with `url`, `title` and the host component of `url`.
void PopulateActivePage(const GURL& url,
                        const std::string& title,
                        events::ContextualCueLogEvent::PageInfo* active_page);

// Fills `cue_details` from the server `cue`: `suggestion_text` is the anchored
// message text, `button_text` the action text and `prompt` the Gemini surface
// prompt, or else the contextual search query, when present. `cuj` and
// `promoted_feature` are copied verbatim.
void PopulateCueDetails(const optimization_guide::proto::ContextualCue& cue,
                        std::string_view cuj,
                        std::string_view promoted_feature,
                        events::ContextualCueLogEvent::CueDetails* cue_details);

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_
