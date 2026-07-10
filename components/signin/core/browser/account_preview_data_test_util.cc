// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_test_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

std::string FormatStatsJson(const DataTypeCounts& counts) {
  return base::StringPrintf(
      R"({
    "dataTypeStatistics": [
      {
        "name": "dataTypes/32904/dataTypeStatistics",
        "count": "%d"
      },
      {
        "name": "dataTypes/45873/dataTypeStatistics",
        "count": "%d"
      },
      {
        "name": "dataTypes/963985/dataTypeStatistics",
        "count": "%d"
      }
    ]
  })",
      counts.bookmark_count, counts.password_count, counts.history_count);
}

std::string FormatPreviewsJson(const std::vector<std::string>& domains) {
  std::vector<std::string> entries;
  for (size_t i = 0; i < domains.size(); ++i) {
    entries.push_back(base::StringPrintf(R"(
      {
        "name": "dataTypes/passwords/syncEntitiesPreviews/%zu",
        "specifics": {
          "passwordPreview": {
            "url": "%s"
          }
        }
      })",
                                         i, domains[i].c_str()));
  }
  return base::StringPrintf(R"({
    "entitiesPreviews": [
      %s
    ]
  })",
                            base::JoinString(entries, ",").c_str());
}

void SimulateSuccessfulStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts) {
  EXPECT_TRUE(test_url_loader_factory->SimulateResponseForPendingRequest(
      kTestStatsUrl, FormatStatsJson(counts)));
}

void SimulateSuccessfulPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::vector<std::string>& domains) {
  EXPECT_TRUE(test_url_loader_factory->SimulateResponseForPendingRequest(
      kTestPreviewsUrl, FormatPreviewsJson(domains)));
}

}  // namespace

void MockSuccessfulStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts) {
  test_url_loader_factory->AddResponse(kTestStatsUrl, FormatStatsJson(counts));
}

void MockSuccessfulPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::vector<std::string>& domains) {
  test_url_loader_factory->AddResponse(kTestPreviewsUrl,
                                       FormatPreviewsJson(domains));
}

void MockSuccessfulFetch(network::TestURLLoaderFactory* test_url_loader_factory,
                         const DataTypeCounts& counts,
                         const std::vector<std::string>& domains) {
  MockSuccessfulStatsFetch(test_url_loader_factory, counts);
  MockSuccessfulPreviewsFetch(test_url_loader_factory, domains);
}

void MockFailedStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    net::Error error_code) {
  network::URLLoaderCompletionStatus status(error_code);
  test_url_loader_factory->AddResponse(
      GURL(kTestStatsUrl), network::mojom::URLResponseHead::New(), "", status);
}

void MockFailedPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    net::Error error_code) {
  network::URLLoaderCompletionStatus status(error_code);
  test_url_loader_factory->AddResponse(GURL(kTestPreviewsUrl),
                                       network::mojom::URLResponseHead::New(),
                                       "", status);
}

void SimulateSuccessfulFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts,
    const std::vector<std::string>& domains) {
  SimulateSuccessfulStatsFetch(test_url_loader_factory, counts);
  SimulateSuccessfulPreviewsFetch(test_url_loader_factory, domains);
}

}  // namespace signin
