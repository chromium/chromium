// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PARSER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PARSER_H_

#include <optional>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "components/signin/public/base/signin_deep_link_payload.h"
#include "url/gurl.h"

namespace signin {

class SigninDeepLinkParser {
 public:
  // Parses the deep link and returns the payload if the deep link is valid
  // format and matches the expected base URL.
  // Otherwise, returns std::nullopt.
  [[nodiscard]] std::optional<SigninDeepLinkPayload> Parse(
      const GURL& deep_link) const;

  // Creates a parser if the cross-device signin deep link feature is enabled
  // and the remote configuration is valid. Otherwise, returns std::nullopt.
  [[nodiscard]] static std::optional<SigninDeepLinkParser>
  CreateForCrossDeviceSigninIfEnabled();

 private:
  // Creates a parser with the expected base URL. The parser will only parse
  // deep links that match the expected base URL.
  [[nodiscard]] explicit SigninDeepLinkParser(GURL expected_base_url)
      : expected_base_url_(std::move(expected_base_url)) {
    CHECK(expected_base_url_.is_valid());
  }

  GURL expected_base_url_;

  FRIEND_TEST_ALL_PREFIXES(SigninDeepLinkParserTest, Parse);
};
}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PARSER_H_
