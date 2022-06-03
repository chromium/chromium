// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/subresource_redirect/proto/robots_rules.pb.h"

namespace subresource_redirect {

namespace {

// Converts the given robots rule pattern to a pattern compatible with
// |base::MatchPattern|.
//
// Robots rule patterns have slightly different semantics than the pattern
// inputs for |base::MatchPattern|. They support '*', which matches zero or more
// of any character, and '$', which matches the end of the input string. On the
// other hand, |base::MatchPattern| supports '*' and '?', zero or one of any
// character, but not '$'.
//
// Both patterns are anchored at the beginning of the input string, but
// |base::MatchPattern| is also implicitly anchored to the end of the string.
// That is, the pattern must match the whole string in order to match.
//
// We can convert the given |robots_rule| to one that is compatible with
// |base::MatchPattern| by taking care of optionally-present '$' character and
// backslash-escaping any '?' characters, since they should be interpreted
// literally .
std::string ConvertRobotsRuleToGlob(const std::string& robots_rule) {
  if (robots_rule.empty())
    return "*";
  std::string glob(robots_rule);
  // Any '\' characters that appear in |robots_rule| are meant as literals. To
  // prevent |base::MatchPattern| from interpreting bare '\' as an escape
  // character, we replace each bare backslash with two backslashes.
  base::ReplaceSubstringsAfterOffset(&glob, 0, "\\", "\\\\");
  // |base::MatchPattern| treats '?' as a special symbol, but robots rule
  // patterns do not. Escape each occurrence with a backslash.
  base::ReplaceSubstringsAfterOffset(&glob, 0, "?", "\\?");
  // |base::MatchPattern| implicitly anchors to the end of the string, but
  // |robots rule patterns require an explicit trailing '$'.
  if (glob.back() == '$')
    glob.pop_back();
  else
    glob.push_back('*');
  return glob;
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
  return base::MatchPattern(path, glob_);
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
    const absl::optional<std::string>& rules) {
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
    robots_rules_.reserve(robots_rules.image_ordered_rules().size());
    std::set<std::string> allowed_pattern_set;
    std::set<std::string> disallowed_pattern_set;
    for (const auto& rule : robots_rules.image_ordered_rules()) {
      if (rule.has_allowed_pattern()) {
        const std::string& pattern = rule.allowed_pattern();
        if (allowed_pattern_set.insert(pattern).second) {
          robots_rules_.emplace_back(true, ConvertRobotsRuleToGlob(pattern));
        }
      } else if (rule.has_disallowed_pattern()) {
        const std::string& pattern = rule.disallowed_pattern();
        if (disallowed_pattern_set.insert(pattern).second) {
          robots_rules_.emplace_back(false, ConvertRobotsRuleToGlob(pattern));
        }
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

absl::optional<RobotsRulesParser::CheckResult>
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
    return absl::nullopt;
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
