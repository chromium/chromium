// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/types.h"

#include <ostream>
#include <tuple>
#include "components/feed/core/proto/v2/ui.pb.h"

namespace feed {

AccountInfo::AccountInfo() = default;
AccountInfo::AccountInfo(const std::string& gaia, const std::string& email)
    : gaia(gaia), email(email) {}
AccountInfo::AccountInfo(CoreAccountInfo account_info)
    : gaia(std::move(account_info.gaia)),
      email(std::move(account_info.email)) {}
bool AccountInfo::IsEmpty() const {
  DCHECK_EQ(gaia.empty(), email.empty());
  return gaia.empty();
}
bool AccountInfo::operator==(const AccountInfo& rhs) const {
  return tie(gaia, email) == tie(rhs.gaia, rhs.email);
}

std::ostream& operator<<(std::ostream& os, const AccountInfo& o) {
  if (o.IsEmpty()) {
    return os << "signed-out";
  }
  return os << o.gaia << ":" << o.email;
}

WebFeedMetadata::WebFeedMetadata() = default;
WebFeedMetadata::WebFeedMetadata(const WebFeedMetadata&) = default;
WebFeedMetadata::WebFeedMetadata(WebFeedMetadata&&) = default;
WebFeedMetadata::~WebFeedMetadata() = default;
WebFeedMetadata& WebFeedMetadata::operator=(const WebFeedMetadata&) = default;
WebFeedMetadata& WebFeedMetadata::operator=(WebFeedMetadata&&) = default;

WebFeedPageInformation::WebFeedPageInformation() = default;
WebFeedPageInformation::~WebFeedPageInformation() = default;
WebFeedPageInformation::WebFeedPageInformation(const WebFeedPageInformation&) =
    default;
WebFeedPageInformation::WebFeedPageInformation(WebFeedPageInformation&&) =
    default;
WebFeedPageInformation& WebFeedPageInformation::operator=(
    const WebFeedPageInformation&) = default;
WebFeedPageInformation& WebFeedPageInformation::operator=(
    WebFeedPageInformation&&) = default;
void WebFeedPageInformation::SetUrl(const GURL& url) {
  GURL::Replacements clear_ref;
  clear_ref.ClearRef();
  url_ = url.ReplaceComponents(clear_ref);
}
void WebFeedPageInformation::SetCanonicalUrl(const GURL& url) {
  GURL::Replacements clear_ref;
  clear_ref.ClearRef();
  canonical_url_ = url.ReplaceComponents(clear_ref);
}
void WebFeedPageInformation::SetRssUrls(const std::vector<GURL>& rss_urls) {
  rss_urls_ = rss_urls;
}

std::ostream& operator<<(std::ostream& os,
                         const WebFeedPageInformation& value) {
  os << "{ " << value.url() << " ";
  if (value.canonical_url().is_valid()) {
    os << "canonical=" << value.canonical_url() << ' ';
  }
  os << "RSS:\n";
  for (const GURL& url : value.GetRssUrls()) {
    os << url << '\n';
  }
  os << "}";
  return os;
}

// operator<< functions below are for test purposes, and shouldn't be called
// from production code to avoid a binary size impact.

std::ostream& operator<<(std::ostream& os, const NetworkResponseInfo& o) {
  return os << "NetworkResponseInfo{"
            << " status_code=" << o.status_code
            << " fetch_duration=" << o.fetch_duration
            << " fetch_time=" << o.fetch_time
            << " bless_nonce=" << o.bless_nonce
            << " base_request_url=" << o.base_request_url
            << " response_body_bytes=" << o.response_body_bytes
            << " account_info=" << o.account_info << "}";
}

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

std::ostream& operator<<(std::ostream& out, WebFeedQueryRequestStatus value) {
  switch (value) {
    case WebFeedQueryRequestStatus::kUnknown:
      return out << "kUnknown";
    case WebFeedQueryRequestStatus::kSuccess:
      return out << "kSuccess";
    case WebFeedQueryRequestStatus::kFailedOffline:
      return out << "kFailedOffline";
    case WebFeedQueryRequestStatus::kFailedUnknownError:
      return out << "kFailedUnknownError";
    case WebFeedQueryRequestStatus::kFailedInvalidUrl:
      return out << "kFailedInvalidUrl";
    case WebFeedQueryRequestStatus::kAbortWebFeedQueryPendingClearAll:
      return out << "kAbortWebFeedQueryPendingClearAll";
  }
}

std::ostream& operator<<(std::ostream& out, WebFeedAvailabilityStatus value) {
  switch (value) {
    case WebFeedAvailabilityStatus::kStateUnspecified:
      return out << "kStateUnspecified";
    case WebFeedAvailabilityStatus::kInactive:
      return out << "kInactive";
    case WebFeedAvailabilityStatus::kActive:
      return out << "kActive";
    case WebFeedAvailabilityStatus::kWaitingForContent:
      return out << "kWaitingForContent";
  }
}

std::ostream& operator<<(std::ostream& out, const WebFeedMetadata& value) {
  out << "WebFeedMetadata{";
  if (!value.web_feed_id.empty())
    out << " id=" << value.web_feed_id;
  if (value.availability_status != WebFeedAvailabilityStatus::kStateUnspecified)
    out << " availability_status=" << value.availability_status;
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

std::ostream& operator<<(std::ostream& out,
                         WebFeedPageInformationRequestReason value) {
  switch (value) {
    case WebFeedPageInformationRequestReason::kUserRequestedFollow:
      return out << "kUserRequestedFollow";
    case WebFeedPageInformationRequestReason::kFollowRecommendation:
      return out << "kFollowRecommendation";
    case WebFeedPageInformationRequestReason::kMenuItemPresentation:
      return out << "kMenuItemPresentation";
  }
}

std::ostream& operator<<(std::ostream& out, SingleWebFeedEntryPoint value) {
  switch (value) {
    case SingleWebFeedEntryPoint::kMenu:
      return out << "kMenu";
    case SingleWebFeedEntryPoint::kAttribution:
      return out << "kAttribution";
    case SingleWebFeedEntryPoint::kRecommendation:
      return out << "kRecommendation";
    case SingleWebFeedEntryPoint::kGroupHeader:
      return out << "kGroupHeader";
    case SingleWebFeedEntryPoint::kOther:
      return out << "kOther";
  }
}

}  // namespace feed
