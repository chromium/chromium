// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/recent_session_observer_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/user_education/recent_session_policy.h"

RecentSessionObserverImpl::RecentSessionObserverImpl(
    Profile& profile,
    std::unique_ptr<RecentSessionPolicy> policy)
    : profile_(profile), policy_(std::move(policy)) {}

RecentSessionObserverImpl::~RecentSessionObserverImpl() = default;

void RecentSessionObserverImpl::Init(RecentSessionTracker& tracker) {
  CHECK(!subscription_);
  subscription_ = tracker.AddRecentSessionsUpdatedCallback(
      base::BindRepeating(&RecentSessionObserverImpl::OnRecentSessionsUpdated,
                          base::Unretained(this)));
}

void RecentSessionObserverImpl::OnRecentSessionsUpdated(
    const RecentSessionData& recent_sessions) {
  policy_->RecordRecentUsageMetrics(recent_sessions);
  if (policy_->ShouldEnableLowUsagePromoMode(recent_sessions)) {
    NotifyLowUsageSession();
  }
}

// Method that will be imported by `UserEducationServiceFactory` to create the
// recent session observer for a profile.
std::unique_ptr<RecentSessionObserver> CreateRecentSessionObserver(
    Profile& profile) {
  return std::make_unique<RecentSessionObserverImpl>(
      profile, std::make_unique<RecentSessionPolicyImpl>());
}
