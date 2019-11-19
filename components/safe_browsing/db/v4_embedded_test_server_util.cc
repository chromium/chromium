// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/v4_embedded_test_server_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/bind.h"
#include "base/logging.h"
#include "components/safe_browsing/db/util.h"
#include "components/safe_browsing/db/v4_test_util.h"
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
// 3. Parsing the FindFullHashesRequest from the decoded string.
std::vector<HashPrefix> GetPrefixesForRequest(const GURL& url) {
  // Find the "req" query param.
  std::string req;
  bool success = net::GetValueForKeyInQuery(url, "$req", &req);
  DCHECK(success) << "Requests to fullHashes:find should include the req param";

  // Base64 decode it.
  std::string decoded_output;
  success = base::Base64UrlDecode(
      req, base::Base64UrlDecodePolicy::REQUIRE_PADDING, &decoded_output);
  DCHECK(success);

  // Parse the FindFullHashRequest from the decoded output.
  FindFullHashesRequest full_hash_req;
  success = full_hash_req.ParseFromString(decoded_output);
  DCHECK(success);

  // Extract HashPrefixes from the request proto.
  const ThreatInfo& info = full_hash_req.threat_info();
  std::vector<HashPrefix> prefixes;
  for (int i = 0; i < info.threat_entries_size(); ++i) {
    prefixes.push_back(info.threat_entries(i).hash());
  }
  return prefixes;
}

// This function listens for requests to /v4/fullHashes:find, and responds with
// predetermined responses.
std::unique_ptr<net::test_server::HttpResponse> HandleFullHashRequest(
    const std::map<GURL, ThreatMatch>& response_map,
    const std::map<GURL, base::TimeDelta>& delay_map,
    const net::test_server::HttpRequest& request) {
  if (!(net::test_server::ShouldHandle(request, "/v4/fullHashes:find")))
    return nullptr;
  FindFullHashesResponse find_full_hashes_response;
  find_full_hashes_response.mutable_negative_cache_duration()->set_seconds(600);

  // Mock a response based on |response_map| and the prefixes scraped from the
  // request URL.
  //
  // This loops through all prefixes requested, and finds all of the full hashes
  // that match the prefix.
  std::vector<HashPrefix> request_prefixes =
      GetPrefixesForRequest(request.GetURL());
  const base::TimeDelta* delay = nullptr;
  for (const HashPrefix& prefix : request_prefixes) {
    for (const auto& response : response_map) {
      FullHash full_hash = V4ProtocolManagerUtil::GetFullHash(response.first);
      if (V4ProtocolManagerUtil::FullHashMatchesHashPrefix(full_hash, prefix)) {
        ThreatMatch* match = find_full_hashes_response.add_matches();
        *match = response.second;
        auto it = delay_map.find(response.first);
        if (it != delay_map.end()) {
          delay = &(it->second);
        }
      }
    }
  }

  std::string serialized_response;
  find_full_hashes_response.SerializeToString(&serialized_response);

  auto http_response =
      (delay ? std::make_unique<net::test_server::DelayedHttpResponse>(*delay)
             : std::make_unique<net::test_server::BasicHttpResponse>());
  http_response->set_content(serialized_response);
  return http_response;
}

}  // namespace

void StartRedirectingV4RequestsForTesting(
    const std::map<GURL, ThreatMatch>& response_map,
    net::test_server::EmbeddedTestServer* embedded_test_server,
    const std::map<GURL, base::TimeDelta>& delay_map) {
  // Static so accessing the underlying buffer won't cause use-after-free.
  static std::string url_prefix;
  url_prefix = embedded_test_server->GetURL("/v4").spec();
  SetSbV4UrlPrefixForTesting(url_prefix.c_str());
  embedded_test_server->RegisterRequestHandler(
      base::BindRepeating(&HandleFullHashRequest, response_map, delay_map));
}

}  // namespace safe_browsing
