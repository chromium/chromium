// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_SEARCH_API_STUB_URL_CHECKER_H_
#define COMPONENTS_SAFE_SEARCH_API_STUB_URL_CHECKER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "net/base/net_errors.h"
#include "services/network/test/test_url_loader_factory.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_search_api {

class URLChecker;

// Helper class to stub out a URLLoaderFactory for use with URLChecker. This
// lets tests control the response the URLChecker will receive from the Safe
// Search API. Used to test both URLChecker itself and classes that use it.
// This class builds a real URLChecker but maintains control over it to set up
// fake responses.
class StubURLChecker {
 public:
  StubURLChecker();

  StubURLChecker(const StubURLChecker&) = delete;
  StubURLChecker& operator=(const StubURLChecker&) = delete;

  ~StubURLChecker();

  // Returns a URLChecker that will use the stubbed-out responses. Can be called
  // before or after setting up the responses.
  std::unique_ptr<URLChecker> BuildURLChecker(size_t cache_size);

  // Sets the stub to return a successful response to all Safe Search API calls
  // from now on.
  void SetUpValidResponse(bool is_porn);

  // Sets the stub to respond to all Safe Search API calls with a failure from
  // now on.
  void SetUpFailedResponse();

  // Clears the stub so it won't return any response from now on.
  void ClearResponses();

 private:
  void SetUpResponse(net::Error error, const std::string& response);

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

}  // namespace safe_search_api

#endif  // COMPONENTS_SAFE_SEARCH_API_STUB_URL_CHECKER_H_
