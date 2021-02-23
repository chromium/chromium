// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "components/subresource_redirect/proto/robots_rules.pb.h"

namespace subresource_redirect {

namespace {

// Returns true if URL path matches the specified pattern. Pattern is anchored
// at the beginning of path. '$' is special only at the end of pattern.
// Algorithm taken from
// https://github.com/google/robotstxt/blob/f465f0ede81099dd8bc4aeb2966b3a892bd488b3/robots.cc#L74
bool IsMatchingRobotsRule(const std::string& path, const std::string& pattern) {
  // Fast path return when pattern is a simple string and not a regex.
  if (pattern.find('*') == std::string::npos &&
      pattern.find('$') == std::string::npos) {
    return base::StartsWith(path, pattern);
  }

  size_t numpos = 1;
  std::vector<size_t> pos(path.length() + 1, 0);

  // The pos[] array holds a sorted list of indexes of 'path', with length
  // 'numpos'.  At the start and end of each iteration of the main loop below,
  // the pos[] array will hold a list of the prefixes of the 'path' which can
  // match the current prefix of 'pattern'. If this list is ever empty,
  // return false. If we reach the end of 'pattern' with at least one element
  // in pos[], return true.

  for (auto pat = pattern.begin(); pat != pattern.end(); ++pat) {
    if (*pat == '$' && pat + 1 == pattern.end()) {
      return (pos[numpos - 1] == path.length());
    }
    if (*pat == '*') {
      numpos = path.length() - pos[0] + 1;
      for (size_t i = 1; i < numpos; i++) {
        pos[i] = pos[i - 1] + 1;
      }
    } else {
      // Includes '$' when not at end of pattern.
      size_t newnumpos = 0;
      for (size_t i = 0; i < numpos; i++) {
        if (pos[i] < path.length() && path[pos[i]] == *pat) {
          pos[newnumpos++] = pos[i] + 1;
        }
      }
      numpos = newnumpos;
      if (numpos == 0)
        return false;
    }
  }
  return true;
}

void RecordRobotsRulesReceiveResultHistogram(
    RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.RobotRulesDecider.ReceiveResult", result);
}

void RecordRobotsRulesApplyDurationHistogram(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("SubresourceRedirect.RobotRulesDecider.ApplyDuration",
                      duration);
}

}  // namespace

bool RobotsRulesParser::RobotsRule::Match(const std::string& path) const {
  return IsMatchingRobotsRule(path, pattern_);
}

RobotsRulesParser::RobotsRulesParser(
    const base::TimeDelta& rules_receive_timeout) {
  // Using base::Unretained(this) is safe here, since the timer
  // |rules_receive_timeout_timer_| is owned by |this| and destroyed before
  // |this|.
  rules_receive_timeout_timer_.Start(
      FROM_HERE, rules_receive_timeout,
      base::BindOnce(&RobotsRulesParser::OnRulesReceiveTimeout,
                     base::Unretained(this)));
  rules_receive_state_ = RulesReceiveState::kTimerRunning;
}

RobotsRulesParser::~RobotsRulesParser() {
  // Consider this as a timeout
  if (rules_receive_timeout_timer_.IsRunning())
    rules_receive_timeout_timer_.FireNow();
}

void RobotsRulesParser::UpdateRobotsRules(
    const base::Optional<std::string>& rules) {
  robots_rules_.clear();
  rules_receive_timeout_timer_.Stop();

  proto::RobotsRules robots_rules;
  bool is_parse_success = rules && robots_rules.ParseFromString(*rules);
  RecordRobotsRulesReceiveResultHistogram(
      is_parse_success
          ? SubresourceRedirectRobotsRulesReceiveResult::kSuccess
          : SubresourceRedirectRobotsRulesReceiveResult::kParseError);
  rules_receive_state_ = is_parse_success ? RulesReceiveState::kSuccess
                                          : RulesReceiveState::kParseFailed;
  if (is_parse_success) {
    robots_rules_.reserve(robots_rules.image_ordered_rules_size());
    for (const auto& rule : robots_rules.image_ordered_rules()) {
      if (rule.has_allowed_pattern()) {
        robots_rules_.emplace_back(true, rule.allowed_pattern());
      } else if (rule.has_disallowed_pattern()) {
        robots_rules_.emplace_back(false, rule.disallowed_pattern());
      }
    }
    UMA_HISTOGRAM_COUNTS_1000("SubresourceRedirect.RobotRulesDecider.Count",
                              robots_rules_.size());
  }

  // Respond to the pending requests, even if robots proto parse failed.
  for (auto& requests : pending_check_requests_) {
    for (auto& request : requests.second) {
      std::move(request.first).Run(CheckRobotsRulesImmediate(request.second));
    }
  }
  pending_check_requests_.clear();
}

base::Optional<RobotsRulesParser::CheckResult>
RobotsRulesParser::CheckRobotsRules(int routing_id,
                                    const GURL& url,
                                    CheckResultCallback callback) {
  std::string path_with_query = url.path();
  if (url.has_query())
    base::StrAppend(&path_with_query, {"?", url.query()});
  if (rules_receive_state_ == RulesReceiveState::kTimerRunning) {
    DCHECK(rules_receive_timeout_timer_.IsRunning());
    auto it = pending_check_requests_.insert(std::make_pair(
        routing_id,
        std::vector<std::pair<CheckResultCallback, std::string>>()));
    it.first->second.emplace_back(
        std::make_pair(std::move(callback), path_with_query));
    return base::nullopt;
  }
  return CheckRobotsRulesImmediate(path_with_query);
}

RobotsRulesParser::CheckResult RobotsRulesParser::CheckRobotsRulesImmediate(
    const std::string& url_path) const {
  if (rules_receive_state_ == RulesReceiveState::kParseFailed)
    return CheckResult::kDisallowed;
  if (rules_receive_state_ == RulesReceiveState::kTimeout)
    return CheckResult::kDisallowedAfterTimeout;
  DCHECK_EQ(rules_receive_state_, RulesReceiveState::kSuccess);

  base::ElapsedTimer rules_apply_timer;
  for (const auto& rule : robots_rules_) {
    if (rule.Match(url_path)) {
      RecordRobotsRulesApplyDurationHistogram(rules_apply_timer.Elapsed());
      return rule.is_allow_rule_ ? CheckResult::kAllowed
                                 : CheckResult::kDisallowed;
    }
  }
  RecordRobotsRulesApplyDurationHistogram(rules_apply_timer.Elapsed());

  // Treat as allowed when none of the allow/disallow rules match.
  return CheckResult::kAllowed;
}

void RobotsRulesParser::OnRulesReceiveTimeout() {
  DCHECK(!rules_receive_timeout_timer_.IsRunning());
  rules_receive_state_ = RulesReceiveState::kTimeout;
  for (auto& requests : pending_check_requests_) {
    for (auto& request : requests.second) {
      std::move(request.first).Run(CheckResult::kTimedout);
    }
  }
  pending_check_requests_.clear();
  RecordRobotsRulesReceiveResultHistogram(
      SubresourceRedirectRobotsRulesReceiveResult::kTimeout);
}

void RobotsRulesParser::InvalidatePendingRequests(int routing_id) {
  auto it = pending_check_requests_.find(routing_id);
  if (it == pending_check_requests_.end())
    return;
  for (auto& request : it->second) {
    std::move(request.first).Run(CheckResult::kInvalidated);
  }
  pending_check_requests_.erase(it);
}

}  // namespace subresource_redirect
