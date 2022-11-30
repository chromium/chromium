// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP_DOMAIN_UTIL_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP_DOMAIN_UTIL_H_

#include <string>

namespace url_formatter {

namespace top_domains {

// Returns true if |hostname| should be considered for an edit distance
// comparison. Will generally return false for short domain names.
bool IsEditDistanceCandidate(const std::string& hostname);

// Returns the portion of hostname without the registry part.
// E.g. For hostname = "google.com", the registry is "com", and the return value
// will be "google".
std::string HostnameWithoutRegistry(const std::string& hostname);

}  // namespace top_domains

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP_DOMAIN_UTIL_H_
