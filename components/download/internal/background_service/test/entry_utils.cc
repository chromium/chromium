// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/guid.h"
#include "components/download/internal/background_service/test/entry_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace download {
namespace test {

bool CompareEntry(const Entry* const& expected, const Entry* const& actual) {
  if (expected == nullptr || actual == nullptr)
    return expected == actual;

  return *expected == *actual;
}

bool CompareEntryList(const std::vector<Entry*>& expected,
                      const std::vector<Entry*>& actual) {
  return std::is_permutation(actual.cbegin(), actual.cend(), expected.cbegin(),
                             CompareEntry);
}

bool CompareEntryList(const std::vector<Entry>& list1,
                      const std::vector<Entry>& list2) {
  return std::is_permutation(list1.begin(), list1.end(), list2.begin());
}

bool CompareEntryUsingGuidOnly(const Entry* const& expected,
                               const Entry* const& actual) {
  if (expected == nullptr || actual == nullptr)
    return expected == actual;

  return expected->guid == actual->guid;
}

bool CompareEntryListUsingGuidOnly(const std::vector<Entry*>& expected,
                                   const std::vector<Entry*>& actual) {
  return std::is_permutation(actual.cbegin(), actual.cend(), expected.cbegin(),
                             CompareEntryUsingGuidOnly);
}

Entry BuildBasicEntry() {
  return BuildEntry(DownloadClient::TEST, base::GenerateGUID());
}

Entry BuildBasicEntry(Entry::State state) {
  Entry entry = BuildBasicEntry();
  entry.state = state;
  if (entry.state == Entry::State::ACTIVE) {
    entry.response_headers =
        base::MakeRefCounted<const net::HttpResponseHeaders>(
            "HTTP/1.1 200 OK\nContent-type: text/html\n\n");
    entry.did_received_response = true;
  }
  return entry;
}

Entry BuildEntry(DownloadClient client, const std::string& guid) {
  Entry entry;
  entry.client = client;
  entry.guid = guid;
  entry.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  return entry;
}

Entry BuildEntry(DownloadClient client,
                 const std::string& guid,
                 base::Time cancel_time,
                 SchedulingParams::NetworkRequirements network_requirements,
                 SchedulingParams::BatteryRequirements battery_requirements,
                 SchedulingParams::Priority priority,
                 const GURL& url,
                 const std::string& request_method,
                 Entry::State state,
                 const base::FilePath& target_file_path,
                 base::Time create_time,
                 base::Time completion_time,
                 base::Time last_cleanup_check_time,
                 uint64_t bytes_downloaded,
                 int attempt_count,
                 int resumption_count) {
  Entry entry = BuildEntry(client, guid);
  entry.scheduling_params.cancel_time = cancel_time;
  entry.scheduling_params.network_requirements = network_requirements;
  entry.scheduling_params.battery_requirements = battery_requirements;
  entry.scheduling_params.priority = priority;
  entry.request_params.url = url;
  entry.request_params.method = request_method;
  entry.state = state;
  entry.target_file_path = target_file_path;
  entry.create_time = create_time;
  entry.completion_time = completion_time;
  entry.last_cleanup_check_time = last_cleanup_check_time;
  entry.bytes_downloaded = bytes_downloaded;
  entry.attempt_count = attempt_count;
  entry.resumption_count = resumption_count;
  entry.url_chain = {url, url};
  entry.response_headers = base::MakeRefCounted<const net::HttpResponseHeaders>(
      "HTTP/1.1 200 OK\nContent-type: text/html\n\n");
  entry.did_received_response = true;
  return entry;
}

}  // namespace test
}  // namespace download
