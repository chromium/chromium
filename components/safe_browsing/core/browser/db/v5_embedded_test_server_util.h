// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_EMBEDDED_TEST_SERVER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_EMBEDDED_TEST_SERVER_UTIL_H_

#include <map>
#include <string>

#include "base/time/time.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "url/gurl.h"

namespace net::test_server {
class EmbeddedTestServer;
}

namespace safe_browsing {

// This method does four things:
// 1. Rewrites the global V5 server URL prefix to point to the test server.
// 2. Registers the hashes:search request handler with the server.
// 3. (Optionally) associates some delay with the resulting http response.
// 4. (Optionally) attaches a cookie to the response.
void StartRedirectingV5RequestsForTesting(
    const std::map<GURL, V5::FullHash>& response_map,
    net::test_server::EmbeddedTestServer* embedded_test_server,
    const std::map<GURL, base::TimeDelta>& delay_map =
        std::map<GURL, base::TimeDelta>(),
    bool serve_cookies = false);

// Rewrites the global V5 server URL prefix to point to `embedded_test_server`
// and registers the hashLists:batchGet request handler with the server.
//  - `hash_lists_map`: Maps list names to V5::HashList responses to return for
//    matching requested lists; other requested lists receive no-op partial
//    updates.
//  - `embedded_test_server`: The test server to register the handler on.
void StartRedirectingV5UpdateRequestsForTesting(
    const std::map<std::string, V5::HashList>& hash_lists_map,
    net::test_server::EmbeddedTestServer* embedded_test_server);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_EMBEDDED_TEST_SERVER_UTIL_H_
