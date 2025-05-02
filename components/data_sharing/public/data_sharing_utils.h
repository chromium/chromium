// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UTILS_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UTILS_H_

#include <optional>

#include "base/types/expected.h"
#include "components/data_sharing/public/group_data.h"
#include "url/gurl.h"

namespace data_sharing {

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.data_sharing)
enum class ParseUrlStatus {
  kUnknown = 0,
  kSuccess = 1,
  kHostOrPathMismatchFailure = 2,
  kQueryMissingFailure = 3
};
using ParseUrlResult = base::expected<GroupToken, ParseUrlStatus>;

class DataSharingUtils {
 public:
  // Check if the given URL should be intercepted
  static bool ShouldInterceptNavigationForShareURL(const GURL& url);

  // Parse and validate a data sharing URL. This simply parses the url. The
  // returned group may not be valid, the caller needs to check ReadGroup or
  // other apis to validate the group.
  static ParseUrlResult ParseDataSharingUrl(const GURL& url);

  // Sets the return value of ShouldInterceptNavigationForShareURL() for tests.
  static void SetShouldInterceptForTesting(
      std::optional<bool> should_intercept_for_testing);

 private:
  static std::optional<bool> should_intercept_for_testing_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UTILS_H_
