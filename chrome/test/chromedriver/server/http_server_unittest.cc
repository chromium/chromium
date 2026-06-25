// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_server.h"

#include <string>
#include <vector>

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/server/http_server_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(HttpServerTest, RequestIsSafeToServe_LocalOnly) {
  net::HttpServerRequestInfo request;
  request.peer = net::IPEndPoint(net::IPAddress::IPv4Localhost(), 12345);

  std::vector<net::IPAddress> whitelisted_ips;
  std::vector<std::string> allowed_origins;

  // Local request (no Origin/Host) -> Allowed.
  EXPECT_TRUE(internal::RequestIsSafeToServe(request, /*allow_remote=*/false,
                                             whitelisted_ips, allowed_origins));

  // Local Origin/Host -> Allowed.
  request.headers["origin"] = "http://localhost:8000";
  request.headers["host"] = "127.0.0.1:9515";
  EXPECT_TRUE(internal::RequestIsSafeToServe(request, /*allow_remote=*/false,
                                             whitelisted_ips, allowed_origins));

  // Non-local Origin -> Rejected.
  request.headers["origin"] = "http://evil.com";
  EXPECT_FALSE(internal::RequestIsSafeToServe(
      request, /*allow_remote=*/false, whitelisted_ips, allowed_origins));

  // Non-local Host -> Rejected.
  request.headers["origin"] = "http://localhost:8000";
  request.headers["host"] = "evil.com";
  EXPECT_FALSE(internal::RequestIsSafeToServe(
      request, /*allow_remote=*/false, whitelisted_ips, allowed_origins));
}

TEST(HttpServerTest, RequestIsSafeToServe_AllowRemote_IPAddressAllowlist) {
  std::vector<net::IPAddress> whitelisted_ips = {
      net::IPAddress::IPv4Localhost(), net::IPAddress(192, 168, 0, 1)};
  std::vector<std::string> allowed_origins;

  // Request from whitelisted IP -> Allowed.
  net::HttpServerRequestInfo request;
  request.peer = net::IPEndPoint(net::IPAddress(192, 168, 0, 1), 12345);
  EXPECT_TRUE(internal::RequestIsSafeToServe(request, /*allow_remote=*/true,
                                             whitelisted_ips, allowed_origins));

  // Request from non-whitelisted IP -> Rejected.
  request.peer = net::IPEndPoint(net::IPAddress(192, 168, 0, 2), 12345);
  EXPECT_FALSE(internal::RequestIsSafeToServe(
      request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));
}

TEST(HttpServerTest, RequestIsSafeToServe_AllowRemote_OriginValidation) {
  net::HttpServerRequestInfo request;
  request.peer = net::IPEndPoint(net::IPAddress(192, 168, 0, 1), 12345);
  request.headers["host"] = "localhost:9515";

  // 1. allowed_origins is empty, whitelisted_ips is empty (allow all remote).
  {
    std::vector<net::IPAddress> whitelisted_ips;
    std::vector<std::string> allowed_origins;

    // Local origin -> Allowed.
    request.headers["origin"] = "http://localhost:8000";
    EXPECT_TRUE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));

    // Non-local origin -> Allowed (default when no origin list and no IP list).
    request.headers["origin"] = "http://evil.com";
    EXPECT_TRUE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));
  }

  // 2. allowed_origins is empty, whitelisted_ips is NOT empty.
  {
    std::vector<net::IPAddress> whitelisted_ips = {
        net::IPAddress::IPv4Localhost(), net::IPAddress(192, 168, 0, 1)};
    std::vector<std::string> allowed_origins;

    // Local origin -> Allowed.
    request.headers["origin"] = "http://localhost:8000";
    EXPECT_TRUE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));

    // Non-local origin -> Rejected (since allowed_origins is empty but
    // whitelisted_ips is not).
    request.headers["origin"] = "http://evil.com";
    EXPECT_FALSE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));
  }

  // 3. allowed_origins is NOT empty, whitelisted_ips is empty (e.g.
  // --allowed-origins=ci.example.com --allowed-ips=).
  {
    std::vector<net::IPAddress> whitelisted_ips;
    std::vector<std::string> allowed_origins = {"ci.example.com"};

    // Local origin -> Allowed.
    request.headers["origin"] = "http://localhost:8000";
    EXPECT_TRUE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));

    // Allowlisted origin -> Allowed.
    request.headers["origin"] = "http://ci.example.com";
    EXPECT_TRUE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));

    // Allowlisted origin with different scheme -> Allowed.
    request.headers["origin"] = "https://ci.example.com";
    EXPECT_TRUE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));

    // Non-allowlisted origin -> Rejected.
    request.headers["origin"] = "http://evil.com";
    EXPECT_FALSE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));
  }

  // 4. allowed_origins contains URL-style origin, whitelisted_ips is empty.
  {
    std::vector<net::IPAddress> whitelisted_ips;
    std::vector<std::string> allowed_origins = {"http://ci.example.com"};

    // Allowlisted origin -> Allowed.
    request.headers["origin"] = "http://ci.example.com";
    EXPECT_TRUE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));

    // Non-allowlisted origin -> Rejected.
    request.headers["origin"] = "http://evil.com";
    EXPECT_FALSE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));
  }

  // 5. allowed_origins contains wildcard "*", whitelisted_ips is empty.
  {
    std::vector<net::IPAddress> whitelisted_ips;
    std::vector<std::string> allowed_origins = {"*"};

    // Any non-local origin -> Allowed.
    request.headers["origin"] = "http://evil.com";
    EXPECT_TRUE(internal::RequestIsSafeToServe(
        request, /*allow_remote=*/true, whitelisted_ips, allowed_origins));
  }
}

TEST(HttpServerTest, HostIsSafeToServe_HostValidation) {
  std::vector<net::IPAddress> whitelisted_ips;
  std::vector<std::string> allowed_origins = {"ci.example.com"};

  // Safe remote host in allowed_origins -> Allowed.
  EXPECT_TRUE(internal::HostIsSafeToServe(GURL("http://ci.example.com"),
                                          "ci.example.com", whitelisted_ips,
                                          allowed_origins));

  // Host in allowed_origins (URL format) -> Allowed.
  std::vector<std::string> allowed_origins_url = {"http://ci.example.com"};
  EXPECT_TRUE(internal::HostIsSafeToServe(GURL("http://ci.example.com"),
                                          "ci.example.com", whitelisted_ips,
                                          allowed_origins_url));

  // Unsafe remote host -> Rejected.
  EXPECT_FALSE(internal::HostIsSafeToServe(GURL("http://evil.com"), "evil.com",
                                           whitelisted_ips, allowed_origins));
}
