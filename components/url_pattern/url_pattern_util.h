// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PATTERN_URL_PATTERN_UTIL_H_
#define COMPONENTS_URL_PATTERN_URL_PATTERN_UTIL_H_

#include <string>
#include <string_view>

#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace url_pattern {

bool ContainsForbiddenHostnameCodePoint(
    std::string_view input,
    const bool allow_ipv6_delimiters = false);

// The following functions are callbacks that may be passed to the
// liburlpattern::Parse() method.  Each performs validation and encoding for
// a different URL component.
// - Canonicalizes a protocol.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-protocol
base::expected<std::string, absl::Status> ProtocolEncodeCallback(
    std::string_view input);
// - Canonicalizes a username.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-username
base::expected<std::string, absl::Status> UsernameEncodeCallback(
    std::string_view input);
// - Canonicalizes a password.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-password
base::expected<std::string, absl::Status> PasswordEncodeCallback(
    std::string_view input);
// - Canonicalizes a hostname.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-hostname
base::expected<std::string, absl::Status> HostnameEncodeCallback(
    std::string_view input);
// - Canonicalizes an IPv6 hostname.
// https://urlpattern.spec.whatwg.org/#canonicalize-an-ipv6-hostname
base::expected<std::string, absl::Status> IPv6HostnameEncodeCallback(
    std::string_view input);
// - Canonicalizes a port.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-port
base::expected<std::string, absl::Status> PortEncodeCallback(
    std::string_view input);
// Note that there are two different pathname callbacks for "standard" URLs
// like `https://foo` // vs "path" URLs like `data:foo`.  Select the correct
// callback depending on the result of `protocol component matches a special
// scheme`.
// https://urlpattern.spec.whatwg.org/#protocol-component-matches-a-special-scheme
// - Canonicalizes a pathname
// https://urlpattern.spec.whatwg.org/#canonicalize-a-pathname
base::expected<std::string, absl::Status> StandardURLPathnameEncodeCallback(
    std::string_view input);
// - Canonicalizes an opaque pathname
// https://urlpattern.spec.whatwg.org/#canonicalize-an-opaque-pathname
base::expected<std::string, absl::Status> PathURLPathnameEncodeCallback(
    std::string_view input);
// - Canonicalizes a search
// https://urlpattern.spec.whatwg.org/#canonicalize-a-search
base::expected<std::string, absl::Status> SearchEncodeCallback(
    std::string_view input);
// - Canonicalizes a hash
// https://urlpattern.spec.whatwg.org/#canonicalize-a-hash
base::expected<std::string, absl::Status> HashEncodeCallback(
    std::string_view input);

// Utility method to determine if a particular hostname pattern should be
// treated as an IPv6 hostname.  This implements a simple and fast heuristic
// looking for a leading `[`.  It is intended to catch the most common cases
// with minimum overhead.
bool TreatAsIPv6Hostname(std::string_view pattern_utf8);

}  // namespace url_pattern

#endif  // COMPONENTS_URL_PATTERN_URL_PATTERN_UTIL_H_
