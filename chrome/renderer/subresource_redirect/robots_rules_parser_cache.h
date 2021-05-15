// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_CACHE_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_CACHE_H_

#include "base/containers/mru_cache.h"
#include "base/memory/weak_ptr.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_redirect {

// The store of robots rules parsers keyed by origin
class RobotsRulesParserCache {
 public:
  // Returns the robots rules parser cache singleton that is shared across the
  // RenderFrames in the renderer.
  static RobotsRulesParserCache& Get();

  RobotsRulesParserCache();
  ~RobotsRulesParserCache();

  RobotsRulesParserCache(const RobotsRulesParserCache&) = delete;
  RobotsRulesParserCache& operator=(const RobotsRulesParserCache&) = delete;

  // Returns if robots rules parser is available for |origin|.
  bool DoRobotsRulesParserExist(const url::Origin& origin);

  // Creates the robots rules parser for |origin| with |rules_receive_timeout|
  // as the timeout value for receiving rules.
  void CreateRobotsRulesParser(const url::Origin& origin,
                               const base::TimeDelta& rules_receive_timeout);

  // Update the robots |rules| to the parser for the |origin|. This update only
  // happens when the cache already has an entry for |origin|.
  void UpdateRobotsRules(const url::Origin& origin,
                         const absl::optional<std::string>& rules);

  // Returns the result of checking whether resource |url| is allowed by robots
  // rules parser for the url origin. When the determination can be made
  // immediately, the decision should be returned. Otherwise absl::nullopt
  // should be returned and the |callback| will be invoked when the decision was
  // made.
  absl::optional<RobotsRulesParser::CheckResult> CheckRobotsRules(
      int routing_id,
      const GURL& url,
      RobotsRulesParser::CheckResultCallback callback);

  // Invalidate and cancel the pending requests for the robots rules parser.
  void InvalidatePendingRequests(int routing_id);

  base::WeakPtr<RobotsRulesParserCache> GetWeakPtr() const {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class SubresourceRedirectLoginRobotsDeciderAgentTest;
  friend class SubresourceRedirectLoginRobotsURLLoaderThrottleTest;

  // The underlying cache of robots rules parsers.
  base::MRUCache<url::Origin, std::unique_ptr<RobotsRulesParser>>
      parsers_cache_;

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<RobotsRulesParserCache> weak_ptr_factory_{this};
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_CACHE_H_
