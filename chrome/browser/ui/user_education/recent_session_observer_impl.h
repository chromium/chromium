// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_RECENT_SESSION_OBSERVER_IMPL_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_RECENT_SESSION_OBSERVER_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/recent_session_policy.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "chrome/browser/user_education/recent_session_observer.h"
#include "chrome/browser/user_education/recent_session_tracker.h"

// Observes recent sessions updates, consults a policy, and:
//  - records metrics
//  - informs user education strategy
//
// Note that tests are in c/b/ui/views/user_education due to the need to have a
// live profile to create an observer.
class RecentSessionObserverImpl : public RecentSessionObserver {
 public:
  RecentSessionObserverImpl(Profile& profile,
                            std::unique_ptr<RecentSessionPolicy> policy);
  ~RecentSessionObserverImpl() override;

  // RecentSessionObserver:
  void Init(RecentSessionTracker& tracker) override;

 private:
  friend class RecentSessionObserverImplTest;

  void OnRecentSessionsUpdated(const RecentSessionData& recent_sessions);

  const raw_ref<Profile> profile_;
  const std::unique_ptr<RecentSessionPolicy> policy_;
  base::CallbackListSubscription subscription_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_RECENT_SESSION_OBSERVER_IMPL_H_
