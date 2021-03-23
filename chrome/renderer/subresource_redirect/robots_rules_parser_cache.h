// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_CACHE_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_CACHE_H_

#include "base/containers/mru_cache.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_redirect {

// The store of robots rules parsers keyed by origin
class RobotsRulesParserCache {
 public:
  RobotsRulesParserCache();
  ~RobotsRulesParserCache();

  RobotsRulesParserCache(const RobotsRulesParserCache&) = delete;
  RobotsRulesParserCache& operator=(const RobotsRulesParserCache&) = delete;

  // Returns if robots rules parser is available for |origin|.
  bool DoRobotsRulesExist(const url::Origin& origin);

  // Update the robots rules to the parser for the |origin|.
  void UpdateRobotsRules(const url::Origin& origin,
                         const base::Optional<std::string>& rules);

  // Returns the result of checking whether resource |url| is allowed by robots
  // rules parser for the url origin. When the determination can be made
  // immediately, the decision should be returned. Otherwise base::nullopt
  // should be returned and the |callback| will be invoked when the decision was
  // made. |rules_receive_timeout| is the timeout value for receiving rules.
  base::Optional<RobotsRulesParser::CheckResult> CheckRobotsRules(
      int routing_id,
      const GURL& url,
      const base::TimeDelta& rules_receive_timeout,
      RobotsRulesParser::CheckResultCallback callback);

  // Invalidate and cancel the pending requests for the robots rules parser.
  void InvalidatePendingRequests(int routing_id);

 private:
  // Returns a reference to the robots rules parser for the |origin| from the
  // cache. An entry is created if it does not exist. |rules_receive_timeout| is
  // the timeout value for receiving rules.
  RobotsRulesParser& GetRobotsRulesParserForOrigin(
      const url::Origin& origin,
      const base::TimeDelta& rules_receive_timeout);

  // The underlying cache of robots rules parsers.
  base::MRUCache<url::Origin, std::unique_ptr<RobotsRulesParser>>
      parsers_cache_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_CACHE_H_
