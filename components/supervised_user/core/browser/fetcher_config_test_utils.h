
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_TEST_UTILS_H_

#include "components/supervised_user/core/browser/kids_external_fetcher_config.h"

namespace supervised_user {

// Test utility for overriding configurations for KidsExternalFetcher.
class FetcherTestConfigBuilder {
 public:
  static FetcherTestConfigBuilder FromConfig(const FetcherConfig& from_config);
  FetcherTestConfigBuilder& WithServiceEndpoint(base::StringPiece value);
  FetcherTestConfigBuilder& WithServicePath(base::StringPiece value);
  FetcherTestConfigBuilder& WithHistogramBasename(base::StringPiece value);
  FetcherConfig Build() const;

 private:
  explicit FetcherTestConfigBuilder(const FetcherConfig& base);
  FetcherConfig config_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_TEST_UTILS_H_
