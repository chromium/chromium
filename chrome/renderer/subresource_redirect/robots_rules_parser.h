// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "url/gurl.h"

namespace subresource_redirect {

// Holds the robots rules for a singe origin, and enables checking whether an
// url path is allowed or disallowed. Also supports a timeout to receive the
// robots rules after which it will be treated as a full disallow. The check
// result is delivered via callback asynchronously.
class RobotsRulesParser {
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
    kAllowed,                 // The resource URL passed the robots rules check
    kDisallowed,              // The resource URL failed the robots rules check
    kTimedout,                // Timeout in retrieving the robots rules
    kDisallowedAfterTimeout,  // Timeout got triggered already, and the resource
                              // was disallowed
    kInvalidated,  // The result check was invalidated, before robots rules are
                   // received or timeout triggered.
  };

  enum class RulesReceiveState {
    // Rules are not received yet, and rules receive timer is still running.
    // This is the default startup state, and is a non-terminal state.
    kTimerRunning,

    // Rules are not received, and rules retrieval timeout happened.
    kTimeout,

    // Rules were received but parsing failed.
    kParseFailed,

    // Rules were received and are parsed successfully.
    kSuccess,
  };

  // Callback to notify the check robot rules result.
  using CheckResultCallback = base::OnceCallback<void(CheckResult)>;

  // |rules_receive_timeout| is the timeout that should be used for receiving
  // the rules.
  explicit RobotsRulesParser(const base::TimeDelta& rules_receive_timeout);

  ~RobotsRulesParser();

  RobotsRulesParser(const RobotsRulesParser&) = delete;
  RobotsRulesParser& operator=(const RobotsRulesParser&) = delete;

  // Update the robots rules. This causes any pending check requests to be
  // processed immediately and called with the result.
  void UpdateRobotsRules(const base::Optional<std::string>& rules);

  // Check whether the URL is allowed or disallowed by robots rules. When the
  // determination can be made immediately, the decision should be returned.
  // Otherwise base::nullopt should be returned and the |callback| will be
  // added to |pending_check_requests_| and called when a decision can be made
  // like when rules are retrieved, or rule fetch timeout, etc.
  // The robots rules check will make use of the |url| path and query
  // parameters.The |url| origin, ref fragment, etc are immaterial. |routing_id|
  // is the render frame ID for which this URL is requested for.
  base::Optional<CheckResult> CheckRobotsRules(int routing_id,
                                               const GURL& url,
                                               CheckResultCallback callback);

  // Invalidate and cancel the pending requests that were added for
  // |routing_id|.
  void InvalidatePendingRequests(int routing_id);

 private:
  friend class SubresourceRedirectRobotsRulesParserTest;

  // Contains one robots.txt rule.
  struct RobotsRule {
    RobotsRule(bool is_allow_rule, const std::string& pattern)
        : is_allow_rule_(is_allow_rule), pattern_(pattern) {}

    bool Match(const std::string& path) const;

    const bool is_allow_rule_;
    const std::string pattern_;
  };

  // Returns the immediate result of whether the URL path is allowed or
  // disallowed by robots rules. Should be called only when rules retrieval
  // state is in a terminal state, i.e., rules receive timer is not running.
  CheckResult CheckRobotsRulesImmediate(const std::string& url_path) const;

  // Called on rules receive timeout. All pending checks for robots rules are
  // notified that the timeout expired and the requests known to |this| are
  // cleared.
  void OnRulesReceiveTimeout();

  // Current state of the rules retrieval.
  RulesReceiveState rules_receive_state_;

  // Ordered list of robots rules from longest to shortest.
  std::vector<RobotsRule> robots_rules_;

  // Contains the requests that are pending for robots rules to be received,
  // keyed by routing ID. Key is the rouging ID and the value holds the URL path
  // and the callback.
  std::map<int, std::vector<std::pair<CheckResultCallback, std::string>>>
      pending_check_requests_;

  // To trigger the timeout for the robots rules to be received.
  base::OneShotTimer rules_receive_timeout_timer_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_ROBOTS_RULES_PARSER_H_
