// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_

#include <string>
#include <vector>

#include "net/base/net_errors.h"

namespace network {
class TestURLLoaderFactory;
}

namespace signin {

// These url matches the non-Stable/Beta urls for testing.
inline constexpr char kTestStatsUrl[] =
    "https://alpha-chromesyncpreview-googleapis.pa.sandbox.google.com/v1/"
    "dataTypes/-/dataTypesStatistics";
inline constexpr char kTestPreviewsUrl[] =
    "https://alpha-chromesyncpreview-googleapis.pa.sandbox.google.com/v1/"
    "dataTypes/-/entitiesPreviews";

struct DataTypeCounts {
  int bookmark_count = 0;
  int password_count = 0;
  int history_count = 0;
};

// Mocks a successful response from the stats endpoint.
void MockSuccessfulStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts = {});

// Mocks a successful response from the previews endpoint.
void MockSuccessfulPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::vector<std::string>& domains = {});

// Mocks successful responses for both stats and previews endpoints.
void MockSuccessfulFetch(network::TestURLLoaderFactory* test_url_loader_factory,
                         const DataTypeCounts& counts = {},
                         const std::vector<std::string>& domains = {});

// Mocks a failed response from the stats endpoint.
void MockFailedStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    net::Error error_code);

// Mocks a failed response from the previews endpoint.
void MockFailedPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    net::Error error_code);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_
