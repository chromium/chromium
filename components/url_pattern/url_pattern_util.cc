// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern/url_pattern_util.h"

#include <ranges>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "url/url_util.h"

namespace url_pattern {
namespace {

std::string StdStringFromCanonOutput(const url::CanonOutput& output,
                                     const url::Component& component) {
  return std::string(UNSAFE_TODO(output.data() + component.begin),
                     component.len);
}

}  // namespace

bool ContainsForbiddenHostnameCodePoint(std::string_view input,
                                        const bool allow_ipv6_delimiters) {
  // The full list of forbidden code points is defined at:
  //
  //  https://url.spec.whatwg.org/#forbidden-host-code-point
  //
  // We only check the code points the chromium URL parser incorrectly permits.
  // See: crbug.com/1065667#c18
  if (!allow_ipv6_delimiters) {
    return std::ranges::any_of(input, [](char c) {
      return c == ' ' || c == '#' || c == ':' || c == '<' || c == '>' ||
             c == '@' || c == '[' || c == ']' || c == '|';
    });
  } else {
    return std::ranges::any_of(input, [](char c) {
      return c == ' ' || c == '#' || c == '<' || c == '>' || c == '@' ||
             c == '|';
    });
  }
}

base::expected<std::string, absl::Status> ProtocolEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  bool result = url::CanonicalizeScheme(input, &canon_output, &component);

  if (!result) {
    return base::unexpected(absl::InvalidArgumentError(
        base::StrCat({"Invalid protocol '", input, "'."})));
  }

  return StdStringFromCanonOutput(canon_output, component);
}

base::expected<std::string, absl::Status> UsernameEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component username_component;
  url::Component password_component;

  bool result =
      url::CanonicalizeUserInfo(input, std::string_view(), &canon_output,
                                &username_component, &password_component);

  if (!result) {
    return base::unexpected(absl::InvalidArgumentError(
        base::StrCat({"Invalid username pattern '", input, "'."})));
  }

  return StdStringFromCanonOutput(canon_output, username_component);
}

base::expected<std::string, absl::Status> PasswordEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component username_component;
  url::Component password_component;

  bool result =
      url::CanonicalizeUserInfo(std::string_view(), input, &canon_output,
                                &username_component, &password_component);

  if (!result) {
    return base::unexpected(absl::InvalidArgumentError(
        base::StrCat({"Invalid password pattern '", input, "'."})));
  }

  return StdStringFromCanonOutput(canon_output, password_component);
}

base::expected<std::string, absl::Status> IPv6HostnameEncodeCallback(
    std::string_view input) {
  std::string result;
  result.reserve(input.size());
  // This implements a light validation and canonicalization of IPv6 hostname
  // content.  Ideally we would use the URL parser's hostname canonicalizer
  // here, but that is too strict for the encoding callback.  The callback may
  // see only bits and pieces of the hostname pattern; e.g. for `[:address]` it
  // sees the `[` and `]` strings as separate calls.  Since the full URL
  // hostname parser wants to completely parse IPv6 hostnames, this will always
  // trigger an error.  Therefore, to allow pattern syntax within IPv6 brackets
  // we simply check for valid characters and lowercase any hex digits.
  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (!base::IsHexDigit(c) && c != '[' && c != ']' && c != ':') {
      return base::unexpected(absl::InvalidArgumentError(
          base::StrCat({"Invalid IPv6 hostname character '",
                        std::string_view(&c, 1), "' in '", input, "'."})));
    }
    result += base::ToLowerASCII(c);
  }
  return result;
}

base::expected<std::string, absl::Status> HostnameEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  // Due to crbug.com/1065667 the url::CanonicalizeHost() call below will
  // permit and possibly encode some illegal code points.  Since we want
  // to ultimately fix that in the future we don't want to encourage more
  // use of these characters in URLPattern.  Therefore we apply an additional
  // restrictive check for these forbidden code points.
  //
  // TODO(crbug.com/40124263): Remove this check after the URL parser is fixed.
  if (ContainsForbiddenHostnameCodePoint(input)) {
    return base::unexpected(absl::InvalidArgumentError(
        base::StrCat({"Invalid hostname pattern '", input, "'."})));
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  bool result = url::CanonicalizeHost(
      input, url::Component(0, base::checked_cast<int>(input.size())),
      &canon_output, &component);

  if (!result) {
    return base::unexpected(absl::InvalidArgumentError(
        base::StrCat({"Invalid hostname pattern '", input, "'."})));
  }

  return StdStringFromCanonOutput(canon_output, component);
}

base::expected<std::string, absl::Status> PortEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  bool result = url::CanonicalizePort(input, url::PORT_UNSPECIFIED,
                                      &canon_output, &component);

  if (!result) {
    return base::unexpected(absl::InvalidArgumentError(
        base::StrCat({"Invalid port pattern '", input, "'."})));
  }

  return StdStringFromCanonOutput(canon_output, component);
}

base::expected<std::string, absl::Status> StandardURLPathnameEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  bool result = url::CanonicalizePartialPath(input, &canon_output, &component);

  if (!result) {
    return base::unexpected(absl::InvalidArgumentError(
        base::StrCat({"Invalid pathname pattern '", input, "'."})));
  }

  return StdStringFromCanonOutput(canon_output, component);
}

base::expected<std::string, absl::Status> PathURLPathnameEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  url::CanonicalizePathUrlPath(input, &canon_output, &component);

  return StdStringFromCanonOutput(canon_output, component);
}

base::expected<std::string, absl::Status> SearchEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  url::CanonicalizeQuery(input, /*converter=*/nullptr, &canon_output,
                         &component);

  return StdStringFromCanonOutput(canon_output, component);
}

base::expected<std::string, absl::Status> HashEncodeCallback(
    std::string_view input) {
  if (input.empty()) {
    return std::string();
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  url::CanonicalizeRef(input, &canon_output, &component);

  return StdStringFromCanonOutput(canon_output, component);
}

// Utility method to determine if a particular hostname pattern should be
// treated as an IPv6 hostname.  This implements a simple and fast heuristic
// looking for a leading `[`.  It is intended to catch the most common cases
// with minimum overhead.
bool TreatAsIPv6Hostname(std::string_view pattern_utf8) {
  // The `[` string cannot be a valid IPv6 hostname.  We need at least two
  // characters to represent `[*`.
  if (pattern_utf8.size() < 2) {
    return false;
  }

  if (pattern_utf8[0] == '[') {
    return true;
  }

  // We do a bit of extra work to detect brackets behind an escape and
  // within a grouping.
  if ((pattern_utf8[0] == '\\' || pattern_utf8[0] == '{') &&
      pattern_utf8[1] == '[') {
    return true;
  }

  return false;
}

}  // namespace url_pattern
