// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/data_sharing_utils.h"

#include "components/data_sharing/public/features.h"
#include "net/base/url_util.h"

namespace data_sharing {
namespace {
constexpr char kGroupIdKey[] = "g";
constexpr char kTokenBlobKey[] = "t";
}  // namespace

std::optional<bool> DataSharingUtils::should_intercept_for_testing_ =
    std::nullopt;

// static
bool DataSharingUtils::ShouldInterceptNavigationForShareURL(const GURL& url) {
  if (should_intercept_for_testing_.has_value()) {
    return should_intercept_for_testing_.value();
  }

  ParseUrlResult result = ParseDataSharingUrl(url);
  if (result.has_value()) {
    return true;
  }
  switch (result.error()) {
    case ParseUrlStatus::kUnknown:
    case ParseUrlStatus::kHostOrPathMismatchFailure:
      return false;
    case ParseUrlStatus::kQueryMissingFailure:
    case ParseUrlStatus::kSuccess:
      return true;
  }
}

// static
ParseUrlResult DataSharingUtils::ParseDataSharingUrl(const GURL& url) {
  GURL data_sharing_url = GURL(data_sharing::features::kDataSharingURL.Get());
  if (url.GetHost() != data_sharing_url.GetHost() ||
      url.GetPath() != data_sharing_url.GetPath()) {
    return base::unexpected(ParseUrlStatus::kHostOrPathMismatchFailure);
  }

  std::string group_id;
  std::string access_token;
  if (!net::GetValueForKeyInQuery(url, kGroupIdKey, &group_id)) {
    group_id.clear();
  }
  if (!net::GetValueForKeyInQuery(url, kTokenBlobKey, &access_token)) {
    access_token.clear();
  }

  if (group_id.empty()) {
    return base::unexpected(ParseUrlStatus::kQueryMissingFailure);
  }

  return base::ok(GroupToken(GroupId(group_id), access_token));
}

// static
void DataSharingUtils::SetShouldInterceptForTesting(
    std::optional<bool> should_intercept_for_testing) {
  should_intercept_for_testing_ = should_intercept_for_testing;
}

}  // namespace data_sharing
