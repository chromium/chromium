// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/signin/core/browser/account_preview_data_fetcher.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace network {
class TestURLLoaderFactory;
}

namespace signin {

// In tests, we dynamically build the URL to represent the same appended params.
inline std::string GetTestStatsUrl() {
  GURL url(
      "https://alpha-chromesyncpreview-googleapis.pa.sandbox.google.com/v1/"
      "dataTypes/-/dataTypesStatistics");
  for (int data_type : signin::kRequestedDataTypes) {
    url = net::AppendQueryParameter(url, "dataTypes",
                                    base::NumberToString(data_type));
  }
  return url.spec();
}
inline constexpr char kTestPreviewsUrl[] =
    "https://alpha-chromesyncpreview-googleapis.pa.sandbox.google.com/v1/"
    "dataTypes/154522/entitiesPreviews";

// Subset of all data types for testing purposes.
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

// Simulates successful responses for the oldest matching pending stats and
// previews fetches.
void SimulateSuccessfulFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts = {},
    const std::vector<std::string>& domains = {});

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_
