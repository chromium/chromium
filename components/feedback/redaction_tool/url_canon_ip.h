// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a copy of url/url_canon_ip.h circa 2023. It should be used only by
// components/feedback/redaction_tool/.
// We need a copy because the components/feedback/redaction_tool source code is
// shared into ChromeOS and needs to have no dependencies outside of base/.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_IP_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_IP_H_

#include "components/feedback/redaction_tool/url_canon.h"
#include "components/feedback/redaction_tool/url_parse.h"

namespace redaction_internal {

// Writes the given IPv4 address to |output|.
void AppendIPv4Address(const unsigned char address[4], CanonOutput* output);

// Writes the given IPv6 address to |output|.
void AppendIPv6Address(const unsigned char address[16], CanonOutput* output);

// Converts an IPv4 address to a 32-bit number (network byte order).
//
// Possible return values:
//   IPV4    - IPv4 address was successfully parsed.
//   BROKEN  - Input was formatted like an IPv4 address, but overflow occurred
//             during parsing.
//   NEUTRAL - Input couldn't possibly be interpreted as an IPv4 address.
//             It might be an IPv6 address, or a hostname.
//
// On success, |num_ipv4_components| will be populated with the number of
// components in the IPv4 address.
CanonHostInfo::Family IPv4AddressToNumber(const char* spec,
                                          const Component& host,
                                          unsigned char address[4],
                                          int* num_ipv4_components);
CanonHostInfo::Family IPv4AddressToNumber(const char16_t* spec,
                                          const Component& host,
                                          unsigned char address[4],
                                          int* num_ipv4_components);

// Converts an IPv6 address to a 128-bit number (network byte order), returning
// true on success. False means that the input was not a valid IPv6 address.
//
// NOTE that |host| is expected to be surrounded by square brackets.
// i.e. "[::1]" rather than "::1".
bool IPv6AddressToNumber(const char* spec,
                         const Component& host,
                         unsigned char address[16]);
bool IPv6AddressToNumber(const char16_t* spec,
                         const Component& host,
                         unsigned char address[16]);

}  // namespace redaction_internal

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_IP_H_
