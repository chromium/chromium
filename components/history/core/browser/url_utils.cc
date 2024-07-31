// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history/core/browser/url_utils.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace history {

namespace {

// Comparator to enforce '\0' < '?' < '#' < '/' < other characters.
int GetURLCharPriority(char ch) {
  switch (ch) {
    case '\0': return 0;
    case '?': return 1;
    case '#': return 2;
    case '/': return 3;
  }
  return 4;
}

}  // namespace

// Instead of splitting URLs and extract path components, we can implement
// CanonicalURLStringCompare() using string operations only. The key idea is,
// treating '/' to be less than any valid path characters would make it behave
// as a separator, so e.g., "test" < "test-case" would be enforced by
// "test/..." < "test-case/...". We also force "?" < "/", so "test?query" <
// "test/stuff". Since the routine is merely lexicographical string comparison
// with remapping of character ordering, so it is a valid strict-weak ordering.
bool CanonicalURLStringCompare(const std::string& s1, const std::string& s2) {
  const std::string::value_type* ch1 = s1.c_str();
  const std::string::value_type* ch2 = s2.c_str();
  while (*ch1 && *ch2 && *ch1 == *ch2) {
    ++ch1;
    ++ch2;
  }
  int pri_diff = GetURLCharPriority(*ch1) - GetURLCharPriority(*ch2);
  // We want false to be returned if `pri_diff` > 0.
  return (pri_diff != 0) ? pri_diff < 0 : *ch1 < *ch2;
}

bool HaveSameSchemeHostAndPort(const GURL&url1, const GURL& url2) {
  return url1.scheme_piece() == url2.scheme_piece() &&
         url1.host_piece() == url2.host_piece() && url1.port() == url2.port();
}

bool IsPathPrefix(const std::string& p1, const std::string& p2) {
  if (p1.length() > p2.length())
    return false;
  std::pair<std::string::const_iterator, std::string::const_iterator>
      first_diff = base::ranges::mismatch(p1, p2);
  // Necessary condition: `p1` is a string prefix of `p2`.
  if (first_diff.first != p1.end())
    return false;  // E.g.: (`p1` = "/test", `p2` = "/exam") => false.

  // `p1` is string prefix.
  if (first_diff.second == p2.end())  // Is exact match?
    return true;  // E.g.: ("/test", "/test") => true.
  // `p1` is strict string prefix, check full match of last path component.
  if (!p1.empty() && *p1.rbegin() == '/')  // Ends in '/'?
    return true;  // E.g.: ("/test/", "/test/stuff") => true.

  // Finally, `p1` does not end in "/": check first extra character in `p2`.
  // E.g.: ("/test", "/test/stuff") => true; ("/test", "/testing") => false.
  return *(first_diff.second) == '/';
}

GURL ToggleHTTPAndHTTPS(const GURL& url) {
  std::string new_scheme;
  if (url.SchemeIs("http")) {
    new_scheme = "https";
  } else if (url.SchemeIs("https")) {
    new_scheme = "http";
  } else {
    return GURL();
  }
  GURL::Replacements replacement;
  replacement.SetSchemeStr(new_scheme);
  return url.ReplaceComponents(replacement);
}

std::string HostForTopHosts(const GURL& url) {
  std::string host = url.host();
  if (base::StartsWith(host, "www.", base::CompareCase::SENSITIVE))
    host.assign(host, 4, std::string::npos);
  return host;
}

}  // namespace history
