// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_logger.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "components/previews/core/previews_logger_observer.h"
#include "components/previews/core/previews_switches.h"

namespace previews {

std::string GetDescriptionForInfoBarDescription(previews::PreviewsType type) {
  return base::StringPrintf("%s InfoBar shown",
                            previews::GetStringNameForType(type).c_str());
}

namespace {

static const char kPreviewDecisionMadeEventType[] = "Decision";
static const char kPreviewNavigationEventType[] = "Navigation";
const size_t kMaximumNavigationLogs = 10;
const size_t kMaximumDecisionLogs = 25;

std::string GetDescriptionForPreviewsNavigation(PreviewsType type,
                                                bool opt_out) {
  return base::StringPrintf("%s preview - user opt-out: %s",
                            GetStringNameForType(type).c_str(),
                            opt_out ? "True" : "False");
}

std::string GetReasonDescription(PreviewsEligibilityReason reason,
                                 bool want_inverse_description) {
  switch (reason) {
    case PreviewsEligibilityReason::ALLOWED:
      DCHECK(!want_inverse_description);
      return "Allowed";
    case PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE:
      return want_inverse_description ? "Blacklist not null"
                                      : "Blacklist failed to be created";
    case PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED:
      return want_inverse_description ? "Blacklist loaded from disk"
                                      : "Blacklist not loaded from disk yet";
    case PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT:
      return want_inverse_description ? "User did not opt out recently"
                                      : "User recently opted out";
    case PreviewsEligibilityReason::USER_BLACKLISTED:
      return want_inverse_description ? "Not all previews are blacklisted"
                                      : "All previews are blacklisted";
    case PreviewsEligibilityReason::HOST_BLACKLISTED:
      return want_inverse_description
                 ? "Host is not blacklisted on all previews"
                 : "All previews on this host are blacklisted";
    case PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE:
      return want_inverse_description ? "Network quality available"
                                      : "Network quality unavailable";
    case PreviewsEligibilityReason::NETWORK_NOT_SLOW:
      return want_inverse_description ? "Network is slow" : "Network not slow";
    case PreviewsEligibilityReason::RELOAD_DISALLOWED:
      return want_inverse_description
                 ? "Page reloads allowed"
                 : "Page reloads do not show previews for this preview type";
    case PreviewsEligibilityReason::DEPRECATED_HOST_BLACKLISTED_BY_SERVER:
      return want_inverse_description ? "Host not blacklisted by server rules"
                                      : "Host blacklisted by server rules";
    case PreviewsEligibilityReason::DEPRECATED_HOST_NOT_WHITELISTED_BY_SERVER:
      return want_inverse_description ? "Host whitelisted by server rules"
                                      : "Host not whitelisted by server rules";
    case PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS:
      return want_inverse_description
                 ? "Not allowed (without server rule check)"
                 : "Allowed (but without server rule check)";
    case PreviewsEligibilityReason::COMMITTED:
      return want_inverse_description ? "Not Committed" : "Committed";
    case PreviewsEligibilityReason::CACHE_CONTROL_NO_TRANSFORM:
      return want_inverse_description
                 ? "Cache-control no-transform not received"
                 : "Cache-control no-transform received";
    case PreviewsEligibilityReason::NETWORK_NOT_SLOW_FOR_SESSION:
      return want_inverse_description
                 ? "Network is slow enough for the session"
                 : "Network not slow enough for the session";
    case PreviewsEligibilityReason::DEVICE_OFFLINE:
      return want_inverse_description ? "Device is online"
                                      : "Device is offline";
    case PreviewsEligibilityReason::URL_HAS_BASIC_AUTH:
      return want_inverse_description
                 ? "URL did not contain basic authentication"
                 : "URL contained basic authentication";
    case PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE:
      return want_inverse_description ? "Optimization hints are available"
                                      : "Optimization hints are not available";
    case PreviewsEligibilityReason::EXCLUDED_BY_MEDIA_SUFFIX:
      return want_inverse_description
                 ? "URL suffix is not an excluded media suffix previews"
                 : "URL suffix is an excluded media suffix";
    case PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE:
      return want_inverse_description ? "Allowed by server rules"
                                      : "Not allowed by server rules";
    case PreviewsEligibilityReason::COINFLIP_HOLDBACK:
      DCHECK(!want_inverse_description);
      return "Coin flip holdback encountered";
    case PreviewsEligibilityReason::REDIRECT_LOOP_DETECTED:
      DCHECK(!want_inverse_description);
      return "Redirect loop detected";
    case PreviewsEligibilityReason::DENY_LIST_MATCHED:
      DCHECK(!want_inverse_description);
      return "URL matched deny list";
    case PreviewsEligibilityReason::LAST:
      break;
  }
  NOTREACHED();
  return "";
}

std::string GetDescriptionForPreviewsDecision(
    PreviewsEligibilityReason reason,
    PreviewsType type,
    bool want_inverse_description = false) {
  return base::StringPrintf(
      "%s preview - %s", GetStringNameForType(type).c_str(),
      GetReasonDescription(reason, want_inverse_description).c_str());
}

}  // namespace

PreviewsLogger::MessageLog::MessageLog(const std::string& event_type,
                                       const std::string& event_description,
                                       const GURL& url,
                                       base::Time time,
                                       uint64_t page_id)
    : event_type(event_type),
      event_description(event_description),
      url(url),
      time(time),
      page_id(page_id) {}

PreviewsLogger::MessageLog::MessageLog(const MessageLog& other)
    : event_type(other.event_type),
      event_description(other.event_description),
      url(other.url),
      time(other.time),
      page_id(other.page_id) {}

PreviewsLogger::PreviewsLogger()
    : blacklist_ignored_(switches::ShouldIgnorePreviewsBlacklist()) {}

PreviewsLogger::~PreviewsLogger() {}

void PreviewsLogger::AddAndNotifyObserver(PreviewsLoggerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
  // Notify the status of blacklist decisions ingored.
  observer->OnIgnoreBlacklistDecisionStatusChanged(blacklist_ignored_);

  // Merge navigation logs and decision logs in chronological order, and push
  // them to |observer|.
  auto navigation_ptr = navigations_logs_.begin();
  auto decision_ptr = decisions_logs_.begin();
  while (navigation_ptr != navigations_logs_.end() ||
         decision_ptr != decisions_logs_.end()) {
    if (navigation_ptr == navigations_logs_.end()) {
      observer->OnNewMessageLogAdded(*decision_ptr);
      ++decision_ptr;
      continue;
    }
    if (decision_ptr == decisions_logs_.end()) {
      observer->OnNewMessageLogAdded(*navigation_ptr);
      ++navigation_ptr;
      continue;
    }
    if (navigation_ptr->time < decision_ptr->time) {
      observer->OnNewMessageLogAdded(*navigation_ptr);
      ++navigation_ptr;
    } else {
      observer->OnNewMessageLogAdded(*decision_ptr);
      ++decision_ptr;
    }
  }

  // Push the current state of blacklist (user blacklisted state and all
  // blacklisted hosts).
  observer->OnUserBlacklistedStatusChange(user_blacklisted_status_);
  for (auto entry : blacklisted_hosts_) {
    observer->OnNewBlacklistedHost(entry.first, entry.second);
  }
}

void PreviewsLogger::RemoveObserver(PreviewsLoggerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
  if (observer_list_.begin() == observer_list_.end()) {
    // |observer_list_| is empty.
    observer->OnLastObserverRemove();
  }
}

void PreviewsLogger::LogMessage(const std::string& event_type,
                                const std::string& event_description,
                                const GURL& url,
                                base::Time time,
                                uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Notify observers about the new MessageLog.
  for (auto& observer : observer_list_) {
    observer.OnNewMessageLogAdded(
        MessageLog(event_type, event_description, url, time, page_id));
  }
}

void PreviewsLogger::LogPreviewNavigation(const GURL& url,
                                          PreviewsType type,
                                          bool opt_out,
                                          base::Time time,
                                          uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(kMaximumNavigationLogs, navigations_logs_.size());

  std::string description = GetDescriptionForPreviewsNavigation(type, opt_out);
  LogMessage(kPreviewNavigationEventType, description, url, time, page_id);

  // Pop out the oldest message when the list is full.
  if (navigations_logs_.size() >= kMaximumNavigationLogs) {
    navigations_logs_.pop_front();
  }

  navigations_logs_.emplace_back(kPreviewNavigationEventType, description, url,
                                 time, page_id);
}

void PreviewsLogger::LogPreviewDecisionMade(
    PreviewsEligibilityReason reason,
    const GURL& url,
    base::Time time,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>&& passed_reasons,
    uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(kMaximumDecisionLogs, decisions_logs_.size());

  // Logs all passed decisions messages.
  for (auto decision : passed_reasons) {
    std::string decision_description = GetDescriptionForPreviewsDecision(
        decision, type, true /* want_inverse_description */);
    LogMessage(kPreviewDecisionMadeEventType, decision_description, url, time,
               page_id);
    decisions_logs_.emplace_back(kPreviewDecisionMadeEventType,
                                 decision_description, url, time, page_id);
  }

  std::string description = GetDescriptionForPreviewsDecision(reason, type);
  LogMessage(kPreviewDecisionMadeEventType, description, url, time, page_id);

  // Pop out the older messages when the list is full.
  while (decisions_logs_.size() >= kMaximumDecisionLogs) {
    decisions_logs_.pop_front();
  }

  decisions_logs_.emplace_back(kPreviewDecisionMadeEventType, description, url,
                               time, page_id);
}

void PreviewsLogger::OnNewBlacklistedHost(const std::string& host,
                                          base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blacklisted_hosts_[host] = time;
  for (auto& observer : observer_list_) {
    observer.OnNewBlacklistedHost(host, time);
  }
}

void PreviewsLogger::OnUserBlacklistedStatusChange(bool blacklisted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_blacklisted_status_ = blacklisted;
  for (auto& observer : observer_list_) {
    observer.OnUserBlacklistedStatusChange(blacklisted);
  }
}

void PreviewsLogger::OnBlacklistCleared(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blacklisted_hosts_.clear();
  navigations_logs_.clear();
  decisions_logs_.clear();
  for (auto& observer : observer_list_) {
    observer.OnBlacklistCleared(time);
  }
}

void PreviewsLogger::OnIgnoreBlacklistDecisionStatusChanged(bool ignored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blacklist_ignored_ = ignored;
  for (auto& observer : observer_list_) {
    observer.OnIgnoreBlacklistDecisionStatusChanged(ignored);
  }
}

}  // namespace previews
