// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/robots_rules_parser_cache.h"

#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"

namespace subresource_redirect {

RobotsRulesParserCache::RobotsRulesParserCache()
    : parsers_cache_(MaxRobotsRulesParsersCacheSize()) {}

RobotsRulesParserCache::~RobotsRulesParserCache() = default;

bool RobotsRulesParserCache::DoRobotsRulesExist(const url::Origin& origin) {
  return parsers_cache_.Get(origin) != parsers_cache_.end();
}

void RobotsRulesParserCache::UpdateRobotsRules(
    const url::Origin& origin,
    const base::Optional<std::string>& rules) {
  GetRobotsRulesParserForOrigin(origin, GetRobotsRulesReceiveTimeout())
      .UpdateRobotsRules(rules);
}

base::Optional<RobotsRulesParser::CheckResult>
RobotsRulesParserCache::CheckRobotsRules(
    int routing_id,
    const GURL& url,
    const base::TimeDelta& rules_receive_timeout,
    RobotsRulesParser::CheckResultCallback callback) {
  return GetRobotsRulesParserForOrigin(url::Origin::Create(url),
                                       rules_receive_timeout)
      .CheckRobotsRules(routing_id, url, std::move(callback));
}

void RobotsRulesParserCache::InvalidatePendingRequests(int routing_id) {
  for (auto& entry : parsers_cache_)
    entry.second->InvalidatePendingRequests(routing_id);
}

RobotsRulesParser& RobotsRulesParserCache::GetRobotsRulesParserForOrigin(
    const url::Origin& origin,
    const base::TimeDelta& rules_receive_timeout) {
  auto it = parsers_cache_.Get(origin);
  if (it == parsers_cache_.end()) {
    it = parsers_cache_.Put(
        origin, std::make_unique<RobotsRulesParser>(rules_receive_timeout));
  }
  return *it->second;
}

}  // namespace subresource_redirect
