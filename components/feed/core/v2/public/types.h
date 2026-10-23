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
#include "google_apis/gaia/gaia_id.h"
#include "url/gurl.h"

namespace feed {

// Information about the user account. Currently, for Feed purposes, we use
// account information only when the user is signed-in with Sync enabled. If
// Sync is disabled, AccountInfo should be empty.
struct AccountInfo {
  AccountInfo();
  AccountInfo(const GaiaId& gaia, const std::string& email);
  explicit AccountInfo(CoreAccountInfo account_info);
  friend bool operator==(const AccountInfo&, const AccountInfo&) = default;
  bool IsEmpty() const;

  GaiaId gaia;
  std::string email;
};
std::ostream& operator<<(std::ostream& os, const AccountInfo& o);

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
  std::string user_feedback_allowed_pref_key;
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

using NetworkRequestId = base::IdTypeU32<class NetworkRequestIdClass>;

// Values for feed type
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed
enum class StreamKind : int {
  // Stream type is unknown.
  kUnknown = 0,
  // For you stream.
  kForYou = 1,
  // Deprecated, as web feed is removed
  // kFollowing = 2,
  kMaxValue = kForYou,
};

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
