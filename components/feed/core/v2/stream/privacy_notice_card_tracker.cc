// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/privacy_notice_card_tracker.h"

#include <string_view>

#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/ios_shared_prefs.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace {

// The number of views of the notice card to consider it acknowledged by the
// user.
const int kViewsCountThreshold = 3;

bool IsPrivacyNoticeCard(const feedwire::ContentId& id) {
  // TODO(b/192015346): This is a less than ideal solution. We're relying on
  // the server to continue serving the notice card with this domain (and not
  // serving other types of cards with this domain). See the bug for the
  // suggested improvement.
  constexpr std::string_view kNoticeCardDomain = "privacynoticecard.f";
  return id.content_domain() == kNoticeCardDomain;
}

}  // namespace

PrivacyNoticeCardTracker::PrivacyNoticeCardTracker(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
  views_count_ = prefs::GetNoticeCardViewsCount(*profile_prefs_);
  has_clicked_ = prefs::GetNoticeCardClicksCount(*profile_prefs_) > 0;
}

void PrivacyNoticeCardTracker::OnCardViewed(
    bool is_signed_in,
    const feedwire::ContentId& content_id) {
  if (!IsPrivacyNoticeCard(content_id))
    return;

  auto now = base::TimeTicks::Now();
  if (now - last_view_time_ < base::Minutes(5))
    return;

  last_view_time_ = now;

  prefs::IncrementNoticeCardViewsCount(*profile_prefs_);
  views_count_++;
}

void PrivacyNoticeCardTracker::OnOpenAction(
    const feedwire::ContentId& content_id) {
  if (!IsPrivacyNoticeCard(content_id) ||
      !prefs::GetLastFetchHadNoticeCard(*profile_prefs_) || has_clicked_) {
    return;
  }

  prefs::IncrementNoticeCardClicksCount(*profile_prefs_);
  has_clicked_ = true;
}

bool PrivacyNoticeCardTracker::HasAcknowledgedNoticeCard() const {
  return has_clicked_ || (views_count_ >= kViewsCountThreshold);
}

}  // namespace feed
