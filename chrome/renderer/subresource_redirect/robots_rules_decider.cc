// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/robots_rules_decider.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "components/data_reduction_proxy/proto/robots_rules.pb.h"

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
  size_t pos[path.length() + 1];

  // The pos[] array holds a sorted list of indexes of 'path', with length
  // 'numpos'.  At the start and end of each iteration of the main loop below,
  // the pos[] array will hold a list of the prefixes of the 'path' which can
  // match the current prefix of 'pattern'. If this list is ever empty,
  // return false. If we reach the end of 'pattern' with at least one element
  // in pos[], return true.

  pos[0] = 0;
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
    RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.RobotRulesDecider.ReceiveResult", result);
}

void RecordRobotsRulesApplyDurationHistogram(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("SubresourceRedirect.RobotRulesDecider.ApplyDuration",
                      duration);
}

}  // namespace

bool RobotsRulesDecider::RobotsRule::Match(const std::string& path) const {
  return IsMatchingRobotsRule(path, pattern_);
}

RobotsRulesDecider::RobotsRulesDecider() {
  // Using base::Unretained(this) is safe here, since the timer
  // |rules_receive_timeout_timer_| is owned by |this| and destroyed before
  // |this|.
  rules_receive_timeout_timer_.Start(
      FROM_HERE, GetRobotsRulesReceiveTimeout(),
      base::BindOnce(&RobotsRulesDecider::OnRulesReceiveTimeout,
                     base::Unretained(this)));
}

RobotsRulesDecider::~RobotsRulesDecider() {
  // Consider this as a timeout
  if (rules_receive_timeout_timer_.IsRunning())
    rules_receive_timeout_timer_.FireNow();
}

void RobotsRulesDecider::UpdateRobotsRules(const std::string& rules) {
  robots_rules_.reset();
  rules_receive_timeout_timer_.Stop();

  proto::RobotsRules robots_rules;
  bool is_parse_success = robots_rules.ParseFromString(rules);
  RecordRobotsRulesReceiveResultHistogram(
      is_parse_success
          ? SubresourceRedirectRobotsRulesReceiveResult::kSuccess
          : SubresourceRedirectRobotsRulesReceiveResult::kParseError);
  if (is_parse_success) {
    robots_rules_ = std::vector<RobotsRule>();
    robots_rules_->reserve(robots_rules.image_ordered_rules_size());
    for (const auto& rule : robots_rules.image_ordered_rules()) {
      if (rule.has_allowed_pattern()) {
        robots_rules_->emplace_back(true, rule.allowed_pattern());
      } else if (rule.has_disallowed_pattern()) {
        robots_rules_->emplace_back(false, rule.disallowed_pattern());
      }
    }
  }
  if (robots_rules_) {
    UMA_HISTOGRAM_COUNTS_1000("SubresourceRedirect.RobotRulesDecider.Count",
                              robots_rules_->size());
  }

  // Respond to the pending requests, even if robots proto parse failed.
  for (auto& request : pending_check_requests_) {
    std::move(request.first)
        .Run(IsAllowed(request.second) ? CheckResult::kAllowed
                                       : CheckResult::kDisallowed);
  }
  pending_check_requests_.clear();
}

void RobotsRulesDecider::CheckRobotsRules(const GURL& url,
                                          CheckResultCallback callback) {
  std::string path_with_query = url.path();
  if (url.has_query())
    base::StrAppend(&path_with_query, {"?", url.query()});
  if (rules_receive_timeout_timer_.IsRunning()) {
    // Rules have not been received yet.
    pending_check_requests_.emplace_back(
        std::make_pair(std::move(callback), path_with_query));
    return;
  }
  std::move(callback).Run(IsAllowed(path_with_query)
                              ? CheckResult::kAllowed
                              : CheckResult::kDisallowed);
}

bool RobotsRulesDecider::IsAllowed(const std::string& url_path) const {
  // Rules not received. Could be rule parse error or timeout.
  if (!robots_rules_)
    return false;

  base::ElapsedTimer rules_apply_timer;
  for (const auto& rule : *robots_rules_) {
    if (rule.Match(url_path)) {
      RecordRobotsRulesApplyDurationHistogram(rules_apply_timer.Elapsed());
      return rule.is_allow_rule_;
    }
  }
  RecordRobotsRulesApplyDurationHistogram(rules_apply_timer.Elapsed());

  // Treat as allowed when none of the allow/disallow rules match.
  return true;
}

void RobotsRulesDecider::OnRulesReceiveTimeout() {
  DCHECK(!rules_receive_timeout_timer_.IsRunning());
  for (auto& request : pending_check_requests_)
    std::move(request.first).Run(CheckResult::kTimedout);
  pending_check_requests_.clear();
  RecordRobotsRulesReceiveResultHistogram(
      SubresourceRedirectRobotsRulesReceiveResult::kTimeout);
}

}  // namespace subresource_redirect
