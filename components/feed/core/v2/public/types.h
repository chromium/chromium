// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "base/types/id_type.h"
#include "base/version.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/version_info/channel.h"
#include "url/gurl.h"

namespace feed {

// Information about the user account. Currently, for Feed purposes, we use
// account information only when the user is signed-in with Sync enabled. If
// Sync is disabled, AccountInfo should be empty.
struct AccountInfo {
  AccountInfo();
  AccountInfo(const std::string& gaia, const std::string& email);
  explicit AccountInfo(CoreAccountInfo account_info);
  bool operator==(const AccountInfo& rhs) const;
  bool operator!=(const AccountInfo& rhs) const { return !(*this == rhs); }
  bool IsEmpty() const;

  std::string gaia;
  std::string email;
};
std::ostream& operator<<(std::ostream& os, const AccountInfo& o);

enum class RefreshTaskId {
  kRefreshForYouFeed,
  // TODO(crbug.com/40158714): Refresh is not currently used for the Web Feed.
  // Remove this code if we don't need it.
  kRefreshWebFeed,
};

enum class AccountTokenFetchStatus {
  // Token fetch was not attempted, or status is unknown.
  kUnspecified = 0,
  // Successfully fetch the correct token.
  kSuccess = 1,
  // The primary account changed before fetching the token completed.
  kUnexpectedAccount = 2,
  // Timed out while fetching the token.
  kTimedOut = 3,
};

// Information about the Chrome build and feature flags.
struct ChromeInfo {
  version_info::Channel channel{};
  base::Version version;
  bool is_new_tab_search_engine_url_android_enabled = false;
};
// Device display metrics.
struct DisplayMetrics {
  float density;
  uint32_t width_pixels;
  uint32_t height_pixels;
};

// A unique ID for an ephemeral change.
using EphemeralChangeId = base::IdTypeU32<class EphemeralChangeIdClass>;
using SurfaceId = base::IdTypeU32<class SurfaceIdClass>;
using ImageFetchId = base::IdTypeU32<class ImageFetchIdClass>;

struct NetworkResponseInfo {
  NetworkResponseInfo();
  NetworkResponseInfo(const NetworkResponseInfo&);
  NetworkResponseInfo(NetworkResponseInfo&&);
  NetworkResponseInfo& operator=(const NetworkResponseInfo&);
  NetworkResponseInfo& operator=(NetworkResponseInfo&&);
  ~NetworkResponseInfo();

  // A union of net::Error (if the request failed) and the http
  // status code(if the request succeeded in reaching the server).
  int32_t status_code = 0;
  base::TimeDelta fetch_duration;
  base::Time fetch_time;
  std::string bless_nonce;
  GURL base_request_url;
  size_t response_body_bytes = 0;
  size_t encoded_size_bytes = 0;
  // If it was a signed-in request, this is the associated account info.
  AccountInfo account_info;
  AccountTokenFetchStatus account_token_fetch_status =
      AccountTokenFetchStatus::kUnspecified;
  base::TimeTicks fetch_time_ticks;
  base::TimeTicks loader_start_time_ticks;
  // List of HTTP response header names and values.
  std::vector<std::string> response_header_names_and_values;
};

std::ostream& operator<<(std::ostream& os, const NetworkResponseInfo& o);

struct NetworkResponse {
  NetworkResponse();
  NetworkResponse(const std::string& response_bytes, int status_code);
  ~NetworkResponse();
  NetworkResponse(const NetworkResponse&);
  NetworkResponse& operator=(const NetworkResponse&);

  // HTTP response body.
  std::string response_bytes;
  // HTTP status code if available, or net::Error otherwise.
  int status_code;
  // List of HTTP response header names and values.
  std::vector<std::string> response_header_names_and_values;
};

// For the snippets-internals page.
struct DebugStreamData {
  static const int kVersion = 1;  // If a field changes, increment.

  DebugStreamData();
  ~DebugStreamData();
  DebugStreamData(const DebugStreamData&);
  DebugStreamData& operator=(const DebugStreamData&);

  std::optional<NetworkResponseInfo> fetch_info;
  std::optional<NetworkResponseInfo> upload_info;
  std::string load_stream_status;
};

std::string SerializeDebugStreamData(const DebugStreamData& data);
std::optional<DebugStreamData> DeserializeDebugStreamData(
    std::string_view base64_encoded);

// Information about a web page which may be used to determine an associated
// web feed.
class WebFeedPageInformation {
 public:
  WebFeedPageInformation();
  ~WebFeedPageInformation();
  WebFeedPageInformation(const WebFeedPageInformation&);
  WebFeedPageInformation(WebFeedPageInformation&&);
  WebFeedPageInformation& operator=(const WebFeedPageInformation&);
  WebFeedPageInformation& operator=(WebFeedPageInformation&&);

  // The URL for the page. `url().has_ref()` is always false.
  const GURL& url() const { return url_; }
  // The Canonical URL for the page, if one was found. `url().has_ref()` is
  // always false
  const GURL& canonical_url() const { return canonical_url_; }
  // The list of RSS urls embedded in the page with the <link> tag.
  const std::vector<GURL>& GetRssUrls() const { return rss_urls_; }

  // Set the URL for the page. Trims off the URL ref.
  void SetUrl(const GURL& url);

  // Set the canonical URL for the page. Trims off the URL ref.
  void SetCanonicalUrl(const GURL& url);

  void SetRssUrls(const std::vector<GURL>& rss_urls);

 private:
  GURL url_;
  GURL canonical_url_;
  std::vector<GURL> rss_urls_;
};
std::ostream& operator<<(std::ostream& os, const WebFeedPageInformation& value);

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.webfeed
enum class WebFeedSubscriptionStatus {
  kUnknown = 0,
  kSubscribed = 1,
  kNotSubscribed = 2,
  kSubscribeInProgress = 3,
  kUnsubscribeInProgress = 4,
};
std::ostream& operator<<(std::ostream& out, WebFeedSubscriptionStatus value);

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.webfeed
enum class WebFeedAvailabilityStatus {
  kStateUnspecified = 0,
  kInactive = 1,
  kActive = 2,
  kWaitingForContent = 4,
};
std::ostream& operator<<(std::ostream& out, WebFeedAvailabilityStatus value);

// Information about a web feed.
struct WebFeedMetadata {
  WebFeedMetadata();
  WebFeedMetadata(const WebFeedMetadata&);
  WebFeedMetadata(WebFeedMetadata&&);
  ~WebFeedMetadata();
  WebFeedMetadata& operator=(const WebFeedMetadata&);
  WebFeedMetadata& operator=(WebFeedMetadata&&);

  // Unique ID of the web feed. Empty if the client knows of no web feed.
  std::string web_feed_id;
  // Whether the subscribed Web Feed has content available for fetching.
  WebFeedAvailabilityStatus availability_status =
      WebFeedAvailabilityStatus::kStateUnspecified;
  // Whether the Web Feed is recommended by the web feeds service.
  bool is_recommended = false;
  std::string title;
  GURL publisher_url;
  WebFeedSubscriptionStatus subscription_status =
      WebFeedSubscriptionStatus::kUnknown;
  GURL favicon_url;
};
std::ostream& operator<<(std::ostream& out, const WebFeedMetadata& value);

// This must be kept in sync with WebFeedSubscriptionRequestStatus in
// enums.xml. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.webfeed
enum class WebFeedSubscriptionRequestStatus {
  kUnknown = 0,
  kSuccess = 1,
  kFailedOffline = 2,
  kFailedTooManySubscriptions = 3,
  kFailedUnknownError = 4,
  kAbortWebFeedSubscriptionPendingClearAll = 5,
  kMaxValue = kAbortWebFeedSubscriptionPendingClearAll,
};
std::ostream& operator<<(std::ostream& out,
                         WebFeedSubscriptionRequestStatus value);

// This must be kept in sync with WebFeedQueryRequestStatus in
// enums.xml. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.webfeed
enum class WebFeedQueryRequestStatus {
  kUnknown = 0,
  kSuccess = 1,
  kFailedOffline = 2,
  kFailedUnknownError = 3,
  kAbortWebFeedQueryPendingClearAll = 4,
  kFailedInvalidUrl = 5,
  kMaxValue = kFailedInvalidUrl,
};
std::ostream& operator<<(std::ostream& out, WebFeedQueryRequestStatus value);

using NetworkRequestId = base::IdTypeU32<class NetworkRequestIdClass>;

// Values for the UMA
// ContentSuggestions.Feed.WebFeed.PageInformationRequested histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with
// WebFeedPageInformationRequestReason in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.webfeed
enum class WebFeedPageInformationRequestReason : int {
  // The user requested to Follow the current web page.
  kUserRequestedFollow = 0,
  // A Follow recommendation is being considered the current web page.
  kFollowRecommendation = 1,
  // The Follow menu item state needs to reflect the current web page.
  kMenuItemPresentation = 2,

  kMaxValue = kMenuItemPresentation,
};

// Values for feed type
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed
enum class StreamKind : int {
  // Stream type is unknown.
  kUnknown = 0,
  // For you stream.
  kForYou = 1,
  // Following stream.
  kFollowing = 2,
  // Single Web Feed (Cormorant) stream.
  kSingleWebFeed = 3,
  // Kid-friendly content stream.
  kSupervisedUser = 4,

  kMaxValue = kSupervisedUser,
};

// Singe Web entry points
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed
enum class SingleWebFeedEntryPoint : int {
  // Three dot menu
  kMenu = 0,
  // Feed Atteribution
  kAttribution = 1,
  // Feed Recomentation
  kRecommendation = 2,
  // Feed Recomentation
  kGroupHeader = 3,
  // Other
  kOther = 4,

  kMaxValue = kOther,
};
std::ostream& operator<<(std::ostream& out, SingleWebFeedEntryPoint value);

// For testing and debugging only.
std::ostream& operator<<(std::ostream& out,
                         WebFeedPageInformationRequestReason value);

// Used to tell how to open an URL.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed
enum class OpenActionType : int {
  // The default open action.
  kDefault = 0,
  // "Open in new tab" action.
  kNewTab = 1,
  // "Open in new tab in group" action.
  kNewTabInGroup = 2,
};

// Describes how tab group feature is enabled.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed
enum class TabGroupEnabledState : int {
  // No tab group is enabled.
  kNone = 0,
  // "Open in new tab in group" replaces "Open in new tab".
  kReplaced = 1,
  // Both "Open in new tab in group" and "Open in new tab" are shown.
  kBoth = 2,
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_
