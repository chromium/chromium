// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/recent_session_tracker.h"
#include <algorithm>
#include <functional>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_storage_service.h"

RecentSessionTracker::RecentSessionTracker(
    user_education::FeaturePromoSessionManager& session_manager,
    user_education::FeaturePromoStorageService& feature_promo_storage,
    RecentSessionDataStorageService& recent_session_storage)
    : subscription_(session_manager.AddNewSessionCallback(
          base::BindRepeating(&RecentSessionTracker::OnSessionStart,
                              base::Unretained(this)))),
      feature_promo_storage_(feature_promo_storage),
      recent_session_storage_(recent_session_storage) {
  if (session_manager.new_session_since_startup()) {
    OnSessionStart();
  }
}

RecentSessionTracker::~RecentSessionTracker() = default;

base::CallbackListSubscription
RecentSessionTracker::AddRecentSessionsUpdatedCallback(
    RecentSessionsUpdatedCallback callback) {
  if (recent_session_data_) {
    callback.Run(*recent_session_data_);
  }
  return recent_sessions_updated_callback_.Add(std::move(callback));
}

void RecentSessionTracker::OnSessionStart() {
  if (!recent_session_data_) {
    recent_session_data_ = recent_session_storage_->ReadRecentSessionData();
  }
  const user_education::FeaturePromoSessionData session =
      feature_promo_storage_->ReadSessionData();
  CHECK(!session.start_time.is_null())
      << "This method should only be called when a new session starts; "
         "therefore the session start time should be valid.";
  RecentSessionData new_data;
  new_data.enabled_time =
      recent_session_data_->enabled_time.value_or(session.start_time);
  new_data.recent_session_start_times.emplace_back(session.start_time);
  for (const auto& start_time :
       recent_session_data_->recent_session_start_times) {
    if (new_data.recent_session_start_times.size() >=
            kMaxRecentSessionRecords ||
        session.start_time - start_time >= kMaxRecentSessionRetention) {
      break;
    }
    // If the system clock has been set back at some point, causing values to
    // be out of order, skip this value. Otherwise record it.
    if (start_time < new_data.recent_session_start_times.back()) {
      new_data.recent_session_start_times.emplace_back(start_time);
    }
  }
  recent_session_storage_->SaveRecentSessionData(new_data);
  recent_session_data_ = new_data;
  recent_sessions_updated_callback_.Notify(new_data);
}
