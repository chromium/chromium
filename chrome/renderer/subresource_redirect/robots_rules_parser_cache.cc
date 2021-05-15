// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/robots_rules_parser_cache.h"

#include "base/no_destructor.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"

namespace subresource_redirect {

// static
RobotsRulesParserCache& RobotsRulesParserCache::Get() {
  static base::NoDestructor<RobotsRulesParserCache> instance;
  return *instance;
}

RobotsRulesParserCache::RobotsRulesParserCache()
    : parsers_cache_(MaxRobotsRulesParsersCacheSize()) {}

RobotsRulesParserCache::~RobotsRulesParserCache() = default;

bool RobotsRulesParserCache::DoRobotsRulesParserExist(
    const url::Origin& origin) {
  return parsers_cache_.Get(origin) != parsers_cache_.end();
}

void RobotsRulesParserCache::CreateRobotsRulesParser(
    const url::Origin& origin,
    const base::TimeDelta& rules_receive_timeout) {
  parsers_cache_.Put(
      origin, std::make_unique<RobotsRulesParser>(rules_receive_timeout));
}

void RobotsRulesParserCache::UpdateRobotsRules(
    const url::Origin& origin,
    const absl::optional<std::string>& rules) {
  // Update the rules when cache has an entry for the origin. It may be missing
  // due to cache eviction.
  auto it = parsers_cache_.Get(origin);
  if (it != parsers_cache_.end())
    it->second->UpdateRobotsRules(rules);
}

absl::optional<RobotsRulesParser::CheckResult>
RobotsRulesParserCache::CheckRobotsRules(
    int routing_id,
    const GURL& url,
    RobotsRulesParser::CheckResultCallback callback) {
  auto it = parsers_cache_.Get(url::Origin::Create(url));
  if (it == parsers_cache_.end()) {
    return RobotsRulesParser::CheckResult::kEntryMissing;
  }
  return it->second->CheckRobotsRules(routing_id, url, std::move(callback));
}

void RobotsRulesParserCache::InvalidatePendingRequests(int routing_id) {
  for (auto& entry : parsers_cache_)
    entry.second->InvalidatePendingRequests(routing_id);
}

}  // namespace subresource_redirect
