// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/types.h"

#include <ostream>

namespace feed {
WebFeedMetadata::WebFeedMetadata() = default;
WebFeedMetadata::WebFeedMetadata(const WebFeedMetadata&) = default;
WebFeedMetadata::WebFeedMetadata(WebFeedMetadata&&) = default;
WebFeedMetadata& WebFeedMetadata::operator=(const WebFeedMetadata&) = default;
WebFeedMetadata& WebFeedMetadata::operator=(WebFeedMetadata&&) = default;

void WebFeedPageInformation::SetUrl(const GURL& url) {
  url::Replacements<char> clear_ref;
  clear_ref.ClearRef();
  url_ = url.ReplaceComponents(clear_ref);
}

// operator<< functions below are for test purposes, and shouldn't be called
// from production code to avoid a binary size impact.

std::ostream& operator<<(std::ostream& out, WebFeedSubscriptionStatus value) {
  switch (value) {
    case WebFeedSubscriptionStatus::kUnknown:
      return out << "kUnknown";
    case WebFeedSubscriptionStatus::kSubscribed:
      return out << "kSubscribed";
    case WebFeedSubscriptionStatus::kNotSubscribed:
      return out << "kNotSubscribed";
    case WebFeedSubscriptionStatus::kSubscribeInProgress:
      return out << "kSubscribeInProgress";
    case WebFeedSubscriptionStatus::kUnsubscribeInProgress:
      return out << "kUnsubscribeInProgress";
  }
}

std::ostream& operator<<(std::ostream& out,
                         WebFeedSubscriptionRequestStatus value) {
  switch (value) {
    case WebFeedSubscriptionRequestStatus::kUnknown:
      return out << "kUnknown";
    case WebFeedSubscriptionRequestStatus::kSuccess:
      return out << "kSuccess";
    case WebFeedSubscriptionRequestStatus::kFailedOffline:
      return out << "kFailedOffline";
    case WebFeedSubscriptionRequestStatus::kFailedTooManySubscriptions:
      return out << "kFailedTooManySubscriptions";
    case WebFeedSubscriptionRequestStatus::kFailedUnknownError:
      return out << "kFailedUnknownError";
    case WebFeedSubscriptionRequestStatus::
        kAbortWebFeedSubscriptionPendingClearAll:
      return out << "kAbortWebFeedSubscriptionPendingClearAll";
  }
}

std::ostream& operator<<(std::ostream& out, const WebFeedMetadata& value) {
  out << "WebFeedMetadata{";
  if (!value.web_feed_id.empty())
    out << " id=" << value.web_feed_id;
  if (value.is_active)
    out << " is_active";
  if (value.is_recommended)
    out << " is_recommended";
  if (!value.title.empty())
    out << " title=" + value.title;
  if (!value.publisher_url.is_empty())
    out << " publisher_url=" << value.publisher_url;
  if (value.subscription_status != WebFeedSubscriptionStatus::kUnknown)
    out << " status=" << value.subscription_status;
  return out << " }";
}

}  // namespace feed
