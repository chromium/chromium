// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_DECIDER_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_DECIDER_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "url/gurl.h"

namespace subresource_redirect {

// Holds the robots rules for a singe origin, and enables checking whether an
// url path is allowed or disallowed. Also supports a timeout to receive the
// robots rules after which it will be treated as a full disallow. The check
// result is delivered via callback asynchronously.
class RobotsRulesDecider {
 public:
  // The final result of robots rule retrieval.
  // This should be kept in sync with
  // SubresourceRedirectRobotsRulesReceiveResult in enums.xml.
  enum SubresourceRedirectRobotsRulesReceiveResult {
    kSuccess,     // Received and parsed successfully
    kTimeout,     // Timeout in waiting for rules
    kParseError,  // Parsing the received binary proto
    kMaxValue = kParseError
  };

  enum CheckResult {
    kAllowed,     // The resource URL passed the robots rules check
    kDisallowed,  // The resource URL failed the robots rules check
    kTimedout,    // Timeout in retrieving the robots rules
  };

  // Callback to notify the check robot rules result.
  using CheckResultCallback = base::OnceCallback<void(CheckResult)>;

  RobotsRulesDecider();
  ~RobotsRulesDecider();

  RobotsRulesDecider(const RobotsRulesDecider&) = delete;
  RobotsRulesDecider& operator=(const RobotsRulesDecider&) = delete;

  // Update the robots rules. This causes any pending check requests to be
  // processed immediately and called with th result.
  void UpdateRobotsRules(const std::string& rules);

  // Check whether the URL is allowed or disallowed by robots rules. |callback|
  // will be called with the result. The callback could be immediate if rules
  // are available. Otherwise the callback will be added to
  // |pending_check_requests_| and called when a decision can be made like when
  // rules are retrieved, or rule fetch timeout, etc.
  // The robots rules check will make use of the |url| path and query
  // parameters. The |url| origin, ref fragment, etc are immaterial.
  void CheckRobotsRules(const GURL& url, CheckResultCallback callback);

 private:
  // Contains one robots.txt rule.
  struct RobotsRule {
    RobotsRule(bool is_allow_rule, const std::string& pattern)
        : is_allow_rule_(is_allow_rule), pattern_(pattern) {}

    bool Match(const std::string& path) const;

    const bool is_allow_rule_;
    const std::string pattern_;
  };

  // Returns if allowed or disallowed by robots rules.
  bool IsAllowed(const std::string& url_path) const;

  // Called on rules receive timeout. All pending checks for robots rules are
  // notified that the timeout expired and the requests known to |this| are
  // cleared.
  void OnRulesReceiveTimeout();

  // The list of robots rules. When this is empty, it could mean either the
  // rules were not received yet, or rules parsing failed.
  base::Optional<std::vector<RobotsRule>> robots_rules_;

  // Contains the requests that are pending for robots rules to be received.
  // Holds the URL path and the callback.
  std::vector<std::pair<CheckResultCallback, std::string>>
      pending_check_requests_;

  // To trigger the timeout for the robots rules to be received.
  base::OneShotTimer rules_receive_timeout_timer_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_DECIDER_H_
