// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

MockEndpointFetcher::MockEndpointFetcher()
    : EndpointFetcher(TRAFFIC_ANNOTATION_FOR_TESTS) {}
MockEndpointFetcher::~MockEndpointFetcher() = default;

void MockEndpointFetcher::SetFetchResponse(
    std::string response_string,
    int http_status_code,
    std::optional<FetchErrorType> error_type) {
  ON_CALL(*this, Fetch)
      .WillByDefault([response_string, http_status_code,
                      error_type](EndpointFetcherCallback callback) {
        std::unique_ptr<EndpointResponse> response =
            std::make_unique<EndpointResponse>();
        response->response = std::move(response_string);
        response->http_status_code = http_status_code;
        if (error_type) {
          response->error_type = error_type;
        }
        std::move(callback).Run(std::move(response));
      });

  ON_CALL(*this, PerformRequest)
      .WillByDefault([response_string, http_status_code, error_type](
                         EndpointFetcherCallback endpoint_fetcher_callback,
                         const char* key) {
        std::unique_ptr<EndpointResponse> response =
            std::make_unique<EndpointResponse>();
        response->response = std::move(response_string);
        response->http_status_code = http_status_code;
        if (error_type) {
          response->error_type = error_type;
        }
        std::move(endpoint_fetcher_callback).Run(std::move(response));
      });
}
