// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_test_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/time.h"
#include "net/http/http_status_code.h"
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
        "name": "dataTypes/%d/dataTypeStatistics",
        "count": "%d"
      },
      {
        "name": "dataTypes/%d/dataTypeStatistics",
        "count": "%d"
      },
      {
        "name": "dataTypes/%d/dataTypeStatistics",
        "count": "%d"
      }
    ]
  })",
      syncer::GetSpecificsFieldNumberFromDataType(syncer::BOOKMARKS),
      counts.bookmark_count,
      syncer::GetSpecificsFieldNumberFromDataType(syncer::PASSWORDS),
      counts.password_count,
      syncer::GetSpecificsFieldNumberFromDataType(syncer::HISTORY),
      counts.history_count);
}

std::string FormatPreviewsJson(const std::vector<DevicePreview>& devices) {
  std::string previews_list;
  for (size_t i = 0; i < devices.size(); ++i) {
    const auto& device = devices[i];
    if (i > 0) {
      previews_list += ",\n";
    }
    previews_list += base::StringPrintf(
        R"({
      "name": "dataTypes/device_info/syncEntitiesPreviews/%zu",
      "specificsPreview": {
        "deviceInfoPreview": {
          "cacheGuid": "%s",
          "lastUpdatedTimestamp": "%lld",
          "osType": %d,
          "deviceFormFactor": %d,
          "chromeVersionInfo": {
            "versionNumber": "126.0.0.0"
          }
        }
      }
    })",
        i, device.cache_guid.c_str(),
        syncer::TimeToProtoTime(device.last_updated),
        static_cast<int>(device.os_type), static_cast<int>(device.form_factor));
  }

  return base::StringPrintf(R"({
  "entitiesPreviews": [
    %s
  ]
})",
                            previews_list.c_str());
}

void SimulateSuccessfulStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts) {
  EXPECT_TRUE(test_url_loader_factory->SimulateResponseForPendingRequest(
      GetTestStatsUrl(), FormatStatsJson(counts)));
}

void SimulateSuccessfulPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::vector<DevicePreview>& devices) {
  EXPECT_TRUE(test_url_loader_factory->SimulateResponseForPendingRequest(
      GetTestPreviewsUrl(), FormatPreviewsJson(devices)));
}

}  // namespace

std::string GetTestStatsUrl() {
  return AccountPreviewDataFetcher::GetStatsUrlForChannel(
             version_info::Channel::UNKNOWN)
      .spec();
}

std::string GetTestPreviewsUrl() {
  return AccountPreviewDataFetcher::GetPreviewsUrlForChannel(
             version_info::Channel::UNKNOWN)
      .spec();
}

void MockSuccessfulStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts) {
  test_url_loader_factory->AddResponse(GetTestStatsUrl(),
                                       FormatStatsJson(counts));
}

void MockSuccessfulPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::vector<DevicePreview>& devices) {
  test_url_loader_factory->AddResponse(GetTestPreviewsUrl(),
                                       FormatPreviewsJson(devices));
}

void MockSuccessfulFetch(network::TestURLLoaderFactory* test_url_loader_factory,
                         const DataTypeCounts& counts,
                         const std::vector<DevicePreview>& devices) {
  MockSuccessfulStatsFetch(test_url_loader_factory, counts);
  MockSuccessfulPreviewsFetch(test_url_loader_factory, devices);
}

void MockFailedStatsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    net::Error error_code) {
  network::URLLoaderCompletionStatus status(error_code);
  test_url_loader_factory->AddResponse(GURL(GetTestStatsUrl()),
                                       network::mojom::URLResponseHead::New(),
                                       "", status);
}

void MockFailedPreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    net::Error error_code) {
  network::URLLoaderCompletionStatus status(error_code);
  test_url_loader_factory->AddResponse(GURL(GetTestPreviewsUrl()),
                                       network::mojom::URLResponseHead::New(),
                                       "", status);
}

void Mock429StatsFetch(network::TestURLLoaderFactory* test_url_loader_factory) {
  test_url_loader_factory->AddResponse(GetTestStatsUrl(), "",
                                       net::HTTP_TOO_MANY_REQUESTS);
}

void Mock429PreviewsFetch(
    network::TestURLLoaderFactory* test_url_loader_factory) {
  test_url_loader_factory->AddResponse(GetTestPreviewsUrl(), "",
                                       net::HTTP_TOO_MANY_REQUESTS);
}

void Mock429Fetch(network::TestURLLoaderFactory* test_url_loader_factory) {
  Mock429StatsFetch(test_url_loader_factory);
  Mock429PreviewsFetch(test_url_loader_factory);
}

void SimulateSuccessfulFetch(
    network::TestURLLoaderFactory* test_url_loader_factory,
    const DataTypeCounts& counts,
    const std::vector<DevicePreview>& devices) {
  SimulateSuccessfulStatsFetch(test_url_loader_factory, counts);
  SimulateSuccessfulPreviewsFetch(test_url_loader_factory, devices);
}

}  // namespace signin
