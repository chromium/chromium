// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_test_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace signin {

void MockSuccessfulStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts) {
  std::string stats_json = base::StringPrintf(
      R"({
    "dataTypeStatistics": [
      {
        "name": "dataTypes/bookmarks/stats",
        "count": "%d"
      },
      {
        "name": "dataTypes/passwords/stats",
        "count": "%d"
      },
      {
        "name": "dataTypes/history/stats",
        "count": "%d"
      }
    ]
  })",
      counts.bookmark_count, counts.password_count, counts.history_count);
  test_url_loader_factory->AddResponse(kTestStatsUrl, stats_json);
}

void MockSuccessfulPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::vector<std::string>& domains) {
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
  std::string previews_json =
      base::StringPrintf(R"({
    "entitiesPreviews": [
      %s
    ]
  })",
                         base::JoinString(entries, ",").c_str());

  test_url_loader_factory->AddResponse(kTestPreviewsUrl, previews_json);
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

}  // namespace signin
