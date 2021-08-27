// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_

#include <iosfwd>
#include <map>
#include <string>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "base/version.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace feed {

enum class RefreshTaskId {
  kRefreshForYouFeed,
  // TODO(1152592): Refresh is not currently used for the Web Feed. Remove this
  // code if we don't need it.
  kRefreshWebFeed,
};

// Information about the Chrome build and feature flags.
struct ChromeInfo {
  version_info::Channel channel{};
  base::Version version;
  bool start_surface;
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

// A map of trial names (key) to group names (value) that is
// sent from the server.
typedef std::map<std::string, std::string> Experiments;

struct NetworkResponseInfo {
  NetworkResponseInfo();
  ~NetworkResponseInfo();
  NetworkResponseInfo(const NetworkResponseInfo&);
  NetworkResponseInfo& operator=(const NetworkResponseInfo&);

  // A union of net::Error (if the request failed) and the http
  // status code(if the request succeeded in reaching the server).
  int32_t status_code = 0;
  base::TimeDelta fetch_duration;
  base::Time fetch_time;
  std::string bless_nonce;
  GURL base_request_url;
  size_t response_body_bytes = 0;
  size_t encoded_size_bytes = 0;
  bool was_signed_in = false;
  base::TimeTicks fetch_time_ticks;
  base::TimeTicks loader_start_time_ticks;
};

std::ostream& operator<<(std::ostream& os, const NetworkResponseInfo& o);

struct NetworkResponse {
  // HTTP response body.
  std::string response_bytes;
  // HTTP status code if available, or net::Error otherwise.
  int status_code;

  NetworkResponse() = default;
  NetworkResponse(NetworkResponse&& other) = default;
  NetworkResponse& operator=(NetworkResponse&& other) = default;
};

// For the snippets-internals page.
struct DebugStreamData {
  static const int kVersion = 1;  // If a field changes, increment.

  DebugStreamData();
  ~DebugStreamData();
  DebugStreamData(const DebugStreamData&);
  DebugStreamData& operator=(const DebugStreamData&);

  absl::optional<NetworkResponseInfo> fetch_info;
  absl::optional<NetworkResponseInfo> upload_info;
  std::string load_stream_status;
};

std::string SerializeDebugStreamData(const DebugStreamData& data);
absl::optional<DebugStreamData> DeserializeDebugStreamData(
    base::StringPiece base64_encoded);

// Information about a web page which may be used to determine an associated web
// feed.
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
  // The list of RSS urls embedded in the page with the <link> tag.
  const std::vector<GURL>& GetRssUrls() const { return rss_urls_; }

  // Set the URL for the page. Trims off the URL ref.
  void SetUrl(const GURL& url);

  void SetRssUrls(const std::vector<GURL>& rss_urls);

 private:
  GURL url_;
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

// This must be kept in sync with WebFeedSubscriptionRequestStatus in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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

using NetworkRequestId = base::IdTypeU32<class NetworkRequestIdClass>;

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_
