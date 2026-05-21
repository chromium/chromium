// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PARSER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PARSER_H_

#include <optional>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "url/gurl.h"

namespace signin {

// The external device entry point for sign-in flow.
// LINT.IfChange(ExternalEntryPoint)
enum class ExternalEntryPoint : int {
  kUnknown = 0,
  kDesktopDefault = 1,
  kMaxValue = kDesktopDefault,
};
// LINT.ThenChange(//components/signin/public/base/signin_deep_link_parser.cc:ExternalEntryPoint)

// The payload of the signin deep link.
struct SigninDeepLinkPayload {
  // The external device entry point ID value read from the deep link.
  // Set to std::nullopt if the entry point ID query parameter is not present or
  // cannot be parsed. Set to kUnknown if the entry point ID was specified as an
  // integer but with unknown value. Otherwise, set to the value of the entry
  // point ID.
  std::optional<ExternalEntryPoint> entry_point_id = std::nullopt;
  // The email address value read from the deep link.
  // Set to std::nullopt if the email address query parameter is not present or
  // cannot be parsed.
  std::optional<std::string> email = std::nullopt;

  bool operator==(const SigninDeepLinkPayload&) const = default;

  // Returns true if the payload has all required fields.
  bool HasAllRequiredFields() const;
};

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
