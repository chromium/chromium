// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_embedded_test_server_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

namespace safe_browsing {

namespace {

// This method parses a request URL and returns a vector of HashPrefixes that
// were being requested. It does this by:
// 1. Finding the "req" query param.
// 2. Base64 decoding it.
// 3. Parsing the SearchHashesRequest from the decoded string.
std::vector<HashPrefixStr> GetPrefixesForRequest(const GURL& url) {
  // Find the "req" query param.
  std::string req;
  bool success = net::GetValueForKeyInQuery(url, "$req", &req);
  CHECK(success) << "Requests to hashes:search should include the req param";

  // Base64 decode it.
  std::string decoded_output;
  success = base::Base64UrlDecode(
      req, base::Base64UrlDecodePolicy::IGNORE_PADDING, &decoded_output);
  CHECK(success);

  // Parse the SearchHashesRequest from the decoded output.
  V5::SearchHashesRequest search_hashes_req;
  success = search_hashes_req.ParseFromString(decoded_output);
  CHECK(success);

  // Extract HashPrefixes from the request proto.
  return std::vector<HashPrefixStr>(search_hashes_req.hash_prefixes().begin(),
                                    search_hashes_req.hash_prefixes().end());
}

// This function listens for requests to /v5/hashes:search, and responds with
// predetermined responses.
std::unique_ptr<net::test_server::HttpResponse> HandleSearchHashesRequest(
    const std::map<GURL, V5::FullHash>& response_map,
    const std::map<GURL, base::TimeDelta>& delay_map,
    bool serve_cookies,
    const net::test_server::HttpRequest& request) {
  if (!(net::test_server::ShouldHandle(request, "/v5/hashes:search"))) {
    return nullptr;
  }
  V5::SearchHashesResponse search_hashes_response;
  search_hashes_response.mutable_cache_duration()->set_seconds(600);

  // Mock a response based on `response_map` and the prefixes scraped from the
  // request URL.
  //
  // This loops through all prefixes requested, and finds all of the full hashes
  // that match the prefix.
  std::vector<HashPrefixStr> request_prefixes =
      GetPrefixesForRequest(request.GetURL());
  const base::TimeDelta* delay = nullptr;
  for (const HashPrefixStr& prefix : request_prefixes) {
    for (const auto& response : response_map) {
      FullHashStr full_hash =
          SBProtocolManagerUtil::GetFullHash(response.first);
      if (SBProtocolManagerUtil::FullHashMatchesHashPrefix(full_hash, prefix)) {
        V5::FullHash* match = search_hashes_response.add_full_hashes();
        *match = response.second;
        auto it = delay_map.find(response.first);
        if (it != delay_map.end()) {
          delay = &(it->second);
        }
      }
    }
  }

  std::string serialized_response;
  search_hashes_response.SerializeToString(&serialized_response);

  auto http_response =
      (delay ? std::make_unique<net::test_server::DelayedHttpResponse>(*delay)
             : std::make_unique<net::test_server::BasicHttpResponse>());
  http_response->set_content(serialized_response);
  if (serve_cookies) {
    http_response->AddCustomHeader("Set-Cookie",
                                   "name=value; SameSite=None; Secure");
  }
  return http_response;
}

}  // namespace

void StartRedirectingV5RequestsForTesting(
    const std::map<GURL, V5::FullHash>& response_map,
    net::test_server::EmbeddedTestServer* embedded_test_server,
    const std::map<GURL, base::TimeDelta>& delay_map,
    bool serve_cookies) {
  // Static so accessing the underlying buffer won't cause use-after-free.
  static base::NoDestructor<std::string> url_prefix;
  *url_prefix = embedded_test_server->GetURL("/v5").spec();
  SetSbV5UrlPrefixForTesting(url_prefix->c_str());
  embedded_test_server->RegisterRequestHandler(base::BindRepeating(
      &HandleSearchHashesRequest, response_map, delay_map, serve_cookies));
}

}  // namespace safe_browsing
