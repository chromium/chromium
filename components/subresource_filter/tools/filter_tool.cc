// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/filter_tool.h"

#include <algorithm>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "components/url_pattern_index/url_rule_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace {

url::Origin ParseOrigin(std::string_view arg) {
  GURL origin_url(arg);
  LOG_IF(FATAL, !origin_url.is_valid()) << "Invalid origin";
  return url::Origin::Create(origin_url);
}

GURL ParseRequestUrl(std::string_view arg) {
  GURL request_url(arg);
  LOG_IF(FATAL, !request_url.is_valid());
  return request_url;
}

url_pattern_index::proto::ElementType ParseType(std::string_view type) {
  // If the user provided a resource type, use it. Else if it's the empty string
  // it will default to ELEMENT_TYPE_OTHER.
  if (type == "other")
    return url_pattern_index::proto::ELEMENT_TYPE_OTHER;
  if (type == "script")
    return url_pattern_index::proto::ELEMENT_TYPE_SCRIPT;
  if (type == "image")
    return url_pattern_index::proto::ELEMENT_TYPE_IMAGE;
  if (type == "stylesheet")
    return url_pattern_index::proto::ELEMENT_TYPE_STYLESHEET;
  if (type == "object")
    return url_pattern_index::proto::ELEMENT_TYPE_OBJECT;
  if (type == "xmlhttprequest")
    return url_pattern_index::proto::ELEMENT_TYPE_XMLHTTPREQUEST;
  if (type == "object_subrequest")
    return url_pattern_index::proto::ELEMENT_TYPE_OBJECT_SUBREQUEST;
  if (type == "subdocument")
    return url_pattern_index::proto::ELEMENT_TYPE_SUBDOCUMENT;
  if (type == "ping")
    return url_pattern_index::proto::ELEMENT_TYPE_PING;
  if (type == "media")
    return url_pattern_index::proto::ELEMENT_TYPE_MEDIA;
  if (type == "font")
    return url_pattern_index::proto::ELEMENT_TYPE_FONT;
  if (type == "popup")
    return url_pattern_index::proto::ELEMENT_TYPE_POPUP;
  if (type == "websocket")
    return url_pattern_index::proto::ELEMENT_TYPE_WEBSOCKET;

  return url_pattern_index::proto::ELEMENT_TYPE_OTHER;
}

const url_pattern_index::flat::UrlRule* FindMatchingUrlRule(
    const subresource_filter::MemoryMappedRuleset* ruleset,
    const url::Origin& document_origin,
    const GURL& request_url,
    url_pattern_index::proto::ElementType type) {
  subresource_filter::mojom::ActivationState state;
  state.activation_level = subresource_filter::mojom::ActivationLevel::kEnabled;
  subresource_filter::DocumentSubresourceFilter filter(document_origin, state,
                                                       ruleset);

  return filter.FindMatchingUrlRule(request_url, type);
}

const std::string& ExtractStringFromDictionary(
    const base::Value::Dict& dictionary,
    const std::string& key) {
  const std::string* found = dictionary.FindString(key);
  CHECK(found);
  return *found;
}

}  // namespace

FilterTool::FilterTool(
    scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset,
    std::ostream* output)
    : ruleset_(std::move(ruleset)), output_(output) {}

FilterTool::~FilterTool() = default;

void FilterTool::Match(const std::string& document_origin,
                       const std::string& url,
                       const std::string& type) {
  bool blocked;
  const url_pattern_index::flat::UrlRule* rule =
      MatchImpl(document_origin, url, type, &blocked);
  PrintResult(blocked, rule, document_origin, url, type);
}

void FilterTool::MatchBatch(std::istream* request_stream) {
  MatchBatchImpl(request_stream, true /* print each request */, 1);
}

void FilterTool::MatchRules(std::istream* request_stream, int min_match_count) {
  MatchBatchImpl(request_stream, false /* print each request */,
                 min_match_count);
}

void FilterTool::PrintResult(bool blocked,
                             const url_pattern_index::flat::UrlRule* rule,
                             std::string_view document_origin,
                             std::string_view url,
                             std::string_view type) {
  *output_ << (blocked ? "BLOCKED " : "ALLOWED ");
  if (rule) {
    *output_ << url_pattern_index::FlatUrlRuleToFilterlistString(rule) << " ";
  }
  *output_ << document_origin << " " << url << " " << type << std::endl;
}

const url_pattern_index::flat::UrlRule* FilterTool::MatchImpl(
    std::string_view document_origin,
    std::string_view url,
    std::string_view type,
    bool* blocked) {
  const url_pattern_index::flat::UrlRule* rule =
      FindMatchingUrlRule(ruleset_.get(), ParseOrigin(document_origin),
                          ParseRequestUrl(url), ParseType(type));

  *blocked = rule && !(rule->options() &
                       url_pattern_index::flat::OptionFlag_IS_ALLOWLIST);
  return rule;
}

// If |print_each_request| is true, then the result of each match is written
// to |output_|, just as in Match. Otherwise, the set of matching rules is
// written to |output_|.
void FilterTool::MatchBatchImpl(std::istream* request_stream,
                                bool print_each_request,
                                int min_match_count) {
  std::unordered_map<const url_pattern_index::flat::UrlRule*, int>
      matched_rules;

  std::string line;
  while (std::getline(*request_stream, line)) {
    if (line.empty())
      continue;

    std::optional<base::Value> dictionary = base::JSONReader::Read(line);
    CHECK(dictionary);

    CHECK(dictionary->is_dict());
    const std::string& origin =
        ExtractStringFromDictionary(dictionary->GetDict(), "origin");
    const std::string& request_url =
        ExtractStringFromDictionary(dictionary->GetDict(), "request_url");
    const std::string& request_type =
        ExtractStringFromDictionary(dictionary->GetDict(), "request_type");

    bool blocked;
    const url_pattern_index::flat::UrlRule* rule =
        MatchImpl(origin, request_url, request_type, &blocked);
    if (rule)
      matched_rules[rule] += 1;

    if (print_each_request)
      PrintResult(blocked, rule, origin, request_url, request_type);
  }

  if (print_each_request)
    return;

  // Sort the rules in descending order by match count.
  std::vector<std::pair<std::string, int>> vector_rules;
  for (auto rule_and_count : matched_rules) {
    if (rule_and_count.second < min_match_count)
      continue;

    vector_rules.push_back(std::make_pair(
        url_pattern_index::FlatUrlRuleToFilterlistString(rule_and_count.first)
            .c_str(),
        rule_and_count.second));
  }

  std::sort(vector_rules.begin(), vector_rules.end(),
            [](const std::pair<std::string, int>& left,
               const std::pair<std::string, int>& right) {
              return left.second > right.second;
            });

  for (auto rule_and_count : vector_rules) {
    *output_ << rule_and_count.second << " " << rule_and_count.first
             << std::endl;
  }
}

}  // namespace subresource_filter
