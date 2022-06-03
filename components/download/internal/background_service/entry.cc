// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/entry.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace download {

namespace {

bool AreHeadersEqual(const net::HttpResponseHeaders* h1,
                     const net::HttpResponseHeaders* h2) {
  if (h1 && h2)
    return h1->raw_headers() == h2->raw_headers();
  return !h1 && !h2;
}

}  // namespace

Entry::Entry()
    : bytes_downloaded(0u),
      bytes_uploaded(0u),
      attempt_count(0),
      resumption_count(0),
      has_upload_data(false),
      did_received_response(false),
      require_response_headers(true) {}
Entry::Entry(const Entry& other) = default;

Entry::Entry(const DownloadParams& params)
    : client(params.client),
      guid(params.guid),
      create_time(base::Time::Now()),
      scheduling_params(params.scheduling_params),
      request_params(params.request_params),
      bytes_downloaded(0u),
      bytes_uploaded(0u),
      attempt_count(0),
      resumption_count(0),
      has_upload_data(false),
      traffic_annotation(params.traffic_annotation),
      did_received_response(false),
      require_response_headers(true) {}

Entry::~Entry() = default;

bool Entry::operator==(const Entry& other) const {
  return client == other.client && guid == other.guid &&
         scheduling_params.cancel_time == other.scheduling_params.cancel_time &&
         scheduling_params.network_requirements ==
             other.scheduling_params.network_requirements &&
         scheduling_params.battery_requirements ==
             other.scheduling_params.battery_requirements &&
         scheduling_params.priority == other.scheduling_params.priority &&
         request_params.url == other.request_params.url &&
         request_params.method == other.request_params.method &&
         request_params.request_headers.ToString() ==
             other.request_params.request_headers.ToString() &&
         state == other.state && target_file_path == other.target_file_path &&
         create_time == other.create_time &&
         completion_time == other.completion_time &&
         last_cleanup_check_time == other.last_cleanup_check_time &&
         bytes_downloaded == other.bytes_downloaded &&
         bytes_uploaded == other.bytes_uploaded &&
         attempt_count == other.attempt_count &&
         resumption_count == other.resumption_count &&
         has_upload_data == other.has_upload_data &&
         traffic_annotation == other.traffic_annotation &&
         url_chain == other.url_chain &&
         AreHeadersEqual(response_headers.get(),
                         other.response_headers.get()) &&
         did_received_response == other.did_received_response &&
         require_response_headers == other.require_response_headers;
}

size_t Entry::EstimateMemoryUsage() const {
  // Ignore size of small primary types and objects.
  return base::trace_event::EstimateMemoryUsage(guid) +
         base::trace_event::EstimateMemoryUsage(request_params.url) +
         base::trace_event::EstimateMemoryUsage(request_params.method) +
         base::trace_event::EstimateMemoryUsage(
             request_params.request_headers.ToString()) +
         base::trace_event::EstimateMemoryUsage(target_file_path.value());
}

}  // namespace download
