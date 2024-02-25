// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENDPOINT_FETCHER_MOCK_ENDPOINT_FETCHER_H_
#define COMPONENTS_ENDPOINT_FETCHER_MOCK_ENDPOINT_FETCHER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

// Used to mock endpoint fetcher in tests.
class MockEndpointFetcher : public EndpointFetcher {
 public:
  MockEndpointFetcher();
  MockEndpointFetcher(const MockEndpointFetcher&) = delete;
  MockEndpointFetcher& operator=(const MockEndpointFetcher&) = delete;
  ~MockEndpointFetcher() override;

  MOCK_METHOD(void, Fetch, (EndpointFetcherCallback callback), (override));
  MOCK_METHOD(void,
              PerformRequest,
              (EndpointFetcherCallback endpoint_fetcher_callback,
               const char* key),
              (override));

  void SetFetchResponse(
      std::string response_string,
      int http_status_code = net::HTTP_OK,
      std::optional<FetchErrorType> error_type = std::nullopt);
};

#endif  // COMPONENTS_ENDPOINT_FETCHER_MOCK_ENDPOINT_FETCHER_H_
