// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_TRACKER_H_
#define CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_TRACKER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_storage_service.h"

// Tracks recent sessions by the current user on this device.
// Used to help understand usage across populations and to provide additional
// input for user education experience triggers.
//
// Data is stored in `RecentSessionDataStorageService`, which is typically part
// of `BrowserFeaturePromoStorageService`.
class RecentSessionTracker {
 public:
  static constexpr int kMaxRecentSessionRecords = 12;
  static constexpr base::TimeDelta kMaxRecentSessionRetention = base::Days(60);

  RecentSessionTracker(
      user_education::FeaturePromoSessionManager& session_manager,
      user_education::FeaturePromoStorageService& feature_promo_storage,
      RecentSessionDataStorageService& recent_session_storage);
  RecentSessionTracker(const RecentSessionTracker&) = delete;
  void operator=(const RecentSessionTracker&) = delete;
  ~RecentSessionTracker();

 private:
  // Called when a new User Education session starts.
  void OnSessionStart();

  const base::CallbackListSubscription subscription_;
  const raw_ref<user_education::FeaturePromoStorageService>
      feature_promo_storage_;
  const raw_ref<RecentSessionDataStorageService> recent_session_storage_;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_TRACKER_H_
