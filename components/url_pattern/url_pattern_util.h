// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PATTERN_URL_PATTERN_UTIL_H_
#define COMPONENTS_URL_PATTERN_URL_PATTERN_UTIL_H_

#include <string>
#include <string_view>

#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace url_pattern {

// The following functions are callbacks that may be passed to the
// liburlpattern::Parse() method.  Each performs validation and encoding for
// a different URL component.
// - Canonicalizes a protocol.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-protocol
absl::StatusOr<std::string> ProtocolEncodeCallback(std::string_view input);
// - Canonicalizes a username.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-username
absl::StatusOr<std::string> UsernameEncodeCallback(std::string_view input);
// - Canonicalizes a password.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-password
absl::StatusOr<std::string> PasswordEncodeCallback(std::string_view input);
// - Canonicalizes a hostname.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-hostname
absl::StatusOr<std::string> HostnameEncodeCallback(std::string_view input);
// - Canonicalizes an IPv6 hostname.
// https://urlpattern.spec.whatwg.org/#canonicalize-an-ipv6-hostname
absl::StatusOr<std::string> IPv6HostnameEncodeCallback(std::string_view input);
// - Canonicalizes a port.
// https://urlpattern.spec.whatwg.org/#canonicalize-a-port
absl::StatusOr<std::string> PortEncodeCallback(std::string_view input);
// Note that there are two different pathname callbacks for "standard" URLs
// like `https://foo` // vs "path" URLs like `data:foo`.  Select the correct
// callback depending on the result of `protocol component matches a special
// scheme`.
// https://urlpattern.spec.whatwg.org/#protocol-component-matches-a-special-scheme
// - Canonicalizes a pathname
// https://urlpattern.spec.whatwg.org/#canonicalize-a-pathname
absl::StatusOr<std::string> StandardURLPathnameEncodeCallback(
    std::string_view input);
// - Canonicalizes an opaque pathname
// https://urlpattern.spec.whatwg.org/#canonicalize-an-opaque-pathname
absl::StatusOr<std::string> PathURLPathnameEncodeCallback(
    std::string_view input);
// - Canonicalizes a search
// https://urlpattern.spec.whatwg.org/#canonicalize-a-search
absl::StatusOr<std::string> SearchEncodeCallback(std::string_view input);
// - Canonicalizes a hash
// https://urlpattern.spec.whatwg.org/#canonicalize-a-hash
absl::StatusOr<std::string> HashEncodeCallback(std::string_view input);

// Utility method to determine if a particular hostname pattern should be
// treated as an IPv6 hostname.  This implements a simple and fast heuristic
// looking for a leading `[`.  It is intended to catch the most common cases
// with minimum overhead.
bool TreatAsIPv6Hostname(std::string_view pattern_utf8);

}  // namespace url_pattern

#endif  // COMPONENTS_URL_PATTERN_URL_PATTERN_UTIL_H_
