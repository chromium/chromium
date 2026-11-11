// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_utils.h"

#include <optional>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
namespace {

// Tests that a bare host is treated as an HTTPS origin.
TEST(RemoteActorUtilsTest, BareHostAssumesHttps) {
  EXPECT_EQ(GetOriginFromDomainOrOrigin("example.com"),
            url::Origin::Create(GURL("https://example.com")));
}

// Tests that the sender's scheme and port are kept, and that the path, query
// and user info are dropped.
TEST(RemoteActorUtilsTest, FullUrlIsCanonicalizedToOrigin) {
  EXPECT_EQ(
      GetOriginFromDomainOrOrigin("http://User:pw@Example.COM:8080/a?b=1"),
      url::Origin::Create(GURL("http://example.com:8080")));
}

// Tests that input which does not form a valid HTTP(S) origin is rejected.
TEST(RemoteActorUtilsTest, InvalidInputIsRejected) {
  constexpr std::string_view kInvalidInputs[] = {
      "",
      "https://",
      "://",
      "http://[invalid]",
      "example.com:invalid_port",
      "wss://example.com",
      "ftp://example.com",
      "file:///etc",
  };
  for (std::string_view input : kInvalidInputs) {
    SCOPED_TRACE(input);
    EXPECT_EQ(GetOriginFromDomainOrOrigin(input), std::nullopt);
  }
}

}  // namespace
}  // namespace password_manager
