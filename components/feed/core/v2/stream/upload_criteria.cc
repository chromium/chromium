// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/upload_criteria.h"

#include "base/feature_list.h"
#include "components/feed/core/v2/ios_shared_prefs.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/stream/notice_card_tracker.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace feed_stream {

UploadCriteria::UploadCriteria(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  UpdateCanUploadActionsWithNoticeCard();
}

bool UploadCriteria::CanUploadActions() const {
  return can_upload_actions_with_notice_card_ ||
         !feed::prefs::GetLastFetchHadNoticeCard(*profile_prefs_);
}

void UploadCriteria::SurfaceOpenedOrClosed() {
  UpdateCanUploadActionsWithNoticeCard();
}

void UploadCriteria::Clear() {
  // Set this to false since we don't know whether there will be a notice card.
  feed::prefs::SetLastFetchHadNoticeCard(*profile_prefs_, true);
  feed::prefs::SetHasReachedClickAndViewActionsUploadConditions(*profile_prefs_,
                                                                false);
  can_upload_actions_with_notice_card_ = false;
}

bool UploadCriteria::HasReachedConditionsToUploadActionsWithNoticeCard() {
  if (base::FeatureList::IsEnabled(
          feed::kInterestFeedV2ClicksAndViewsConditionalUpload)) {
    return feed::prefs::GetHasReachedClickAndViewActionsUploadConditions(
        *profile_prefs_);
  }
  // Consider the conditions as already reached to enable uploads when the
  // feature is disabled. This will also have the effect of not updating the
  // related pref.
  return true;
}

void UploadCriteria::UpdateCanUploadActionsWithNoticeCard() {
  can_upload_actions_with_notice_card_ =
      HasReachedConditionsToUploadActionsWithNoticeCard();
}

}  // namespace feed_stream
}  // namespace feed
