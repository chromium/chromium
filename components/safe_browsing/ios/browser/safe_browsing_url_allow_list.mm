// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"

#include "base/no_destructor.h"
#import "ios/web/public/web_state.h"

using safe_browsing::SBThreatType;

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingUrlAllowList)

// static
GURL SafeBrowsingUrlAllowList::GetDecisionUrl(
    const security_interstitials::UnsafeResource& resource) {
  return resource.navigation_url.is_valid() ? resource.navigation_url
                                            : resource.url;
}

SafeBrowsingUrlAllowList::SafeBrowsingUrlAllowList(web::WebState* web_state)
    : web_state_(web_state) {}

SafeBrowsingUrlAllowList::~SafeBrowsingUrlAllowList() {
  for (auto& observer : observers_) {
    observer.SafeBrowsingAllowListDestroyed(this);
  }
}

void SafeBrowsingUrlAllowList::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SafeBrowsingUrlAllowList::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SafeBrowsingUrlAllowList::AreUnsafeNavigationsAllowed(
    const GURL& url,
    std::set<SBThreatType>* threat_types) const {
  return ContainsThreats(url, Policy::kAllowed, threat_types);
}

void SafeBrowsingUrlAllowList::AllowUnsafeNavigations(
    const GURL& url,
    SBThreatType threat_type) {
  SetThreatPolicy(url, threat_type, Policy::kAllowed);
}

void SafeBrowsingUrlAllowList::DisallowUnsafeNavigations(const GURL& url) {
  RevertPolicy(url, Policy::kAllowed);
}

bool SafeBrowsingUrlAllowList::IsUnsafeNavigationDecisionPending(
    const GURL& url,
    std::set<SBThreatType>* threat_types) const {
  return ContainsThreats(url, Policy::kPending, threat_types);
}

void SafeBrowsingUrlAllowList::AddPendingUnsafeNavigationDecision(
    const GURL& url,
    SBThreatType threat_type) {
  SetThreatPolicy(url, threat_type, Policy::kPending);
}

void SafeBrowsingUrlAllowList::RemovePendingUnsafeNavigationDecisions(
    const GURL& url) {
  RevertPolicy(url, Policy::kPending);
}

#pragma mark - Private

SafeBrowsingUrlAllowList::UnsafeNavigationDecisions&
SafeBrowsingUrlAllowList::GetUnsafeNavigationDecisions(const GURL& url) {
  return decisions_[url.GetWithEmptyPath()];
}

const SafeBrowsingUrlAllowList::UnsafeNavigationDecisions&
SafeBrowsingUrlAllowList::GetUnsafeNavigationDecisions(const GURL& url) const {
  static const base::NoDestructor<UnsafeNavigationDecisions> kEmptyDecisions;
  const auto& it = decisions_.find(url.GetWithEmptyPath());
  if (it == decisions_.end())
    return *kEmptyDecisions;
  return it->second;
}

void SafeBrowsingUrlAllowList::SetThreatPolicy(
    const GURL& url,
    safe_browsing::SBThreatType threat_type,
    Policy policy) {
  auto& decisions = GetUnsafeNavigationDecisions(url);
  if (policy == Policy::kDisallowed) {
    decisions.allowed_threats.erase(threat_type);
    decisions.pending_threats.erase(threat_type);
  }
  if (policy == Policy::kPending) {
    decisions.allowed_threats.erase(threat_type);
    decisions.pending_threats.insert(threat_type);
  }
  if (policy == Policy::kAllowed) {
    decisions.allowed_threats.insert(threat_type);
    decisions.pending_threats.erase(threat_type);
  }
  for (auto& observer : observers_) {
    observer.ThreatPolicyUpdated(this, url, threat_type, policy);
  }
  web_state_->DidChangeVisibleSecurityState();
}

bool SafeBrowsingUrlAllowList::ContainsThreats(
    const GURL& url,
    Policy policy,
    std::set<SBThreatType>* threat_types) const {
  const std::set<SBThreatType>* threats_with_policy = nullptr;
  switch (policy) {
    case Policy::kAllowed:
      threats_with_policy = &GetUnsafeNavigationDecisions(url).allowed_threats;
      break;
    case Policy::kPending:
      threats_with_policy = &GetUnsafeNavigationDecisions(url).pending_threats;
      break;
    case Policy::kDisallowed:
      break;
  }
  if (threats_with_policy && !threats_with_policy->empty()) {
    if (threat_types)
      *threat_types = *threats_with_policy;
    return true;
  }
  return false;
}

void SafeBrowsingUrlAllowList::RevertPolicy(const GURL& url, Policy policy) {
  std::set<SBThreatType>* decisions_to_revert = nullptr;
  switch (policy) {
    case Policy::kAllowed:
      decisions_to_revert = &GetUnsafeNavigationDecisions(url).allowed_threats;
      break;
    case Policy::kPending:
      decisions_to_revert = &GetUnsafeNavigationDecisions(url).pending_threats;
      break;
    case Policy::kDisallowed:
      break;
  }
  if (!decisions_to_revert)
    return;

  std::set<SBThreatType> disallowed_threats;
  disallowed_threats.swap(*decisions_to_revert);
  for (auto& observer : observers_) {
    observer.ThreatPolicyBatchUpdated(this, url, disallowed_threats,
                                      Policy::kDisallowed);
  }
  web_state_->DidChangeVisibleSecurityState();
}

#pragma mark - SafeBrowsingUrlAllowList::UnsafeNavigationDecisions

SafeBrowsingUrlAllowList::UnsafeNavigationDecisions::
    UnsafeNavigationDecisions() = default;

SafeBrowsingUrlAllowList::UnsafeNavigationDecisions::
    ~UnsafeNavigationDecisions() = default;
