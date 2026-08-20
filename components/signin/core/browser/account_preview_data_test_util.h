// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_

#include <string>
#include <vector>

#include "components/signin/core/browser/account_preview_data_fetcher.h"
#include "net/base/net_errors.h"

namespace network {
class TestURLLoaderFactory;
}

namespace signin {

struct DevicePreview;

// In tests, we dynamically build the URL to represent the same appended params.
std::string GetTestStatsUrl();
std::string GetTestPreviewsUrl();

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
    const std::vector<DevicePreview>& devices = {});

// Mocks successful responses for both stats and previews endpoints.
void MockSuccessfulFetch(network::TestURLLoaderFactory* test_url_loader_factory,
                         const DataTypeCounts& counts = {},
                         const std::vector<DevicePreview>& devices = {});

// Mocks a failed response from the stats endpoint.
void MockFailedStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    net::Error error_code);

// Mocks a failed response from the previews endpoint.
void MockFailedPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    net::Error error_code);

// Mocks a 429 Too Many Requests response from the stats endpoint.
void Mock429StatsFetch(network::TestURLLoaderFactory* test_url_loader_factory);

// Mocks a 429 Too Many Requests response from the previews endpoint.
void Mock429PreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory);

// Mocks 429 Too Many Requests responses for both stats and previews endpoints.
void Mock429Fetch(network::TestURLLoaderFactory* test_url_loader_factory);

// Simulates successful responses for the oldest matching pending stats and
// previews fetches.
void SimulateSuccessfulFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts = {},
    const std::vector<DevicePreview>& devices = {});

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_TEST_UTIL_H_
