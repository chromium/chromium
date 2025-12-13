// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_utils.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace history {

bool HaveSameSchemeHostAndPort(const GURL&url1, const GURL& url2) {
  return url1.scheme() == url2.scheme() && url1.host() == url2.host() &&
         url1.GetPort() == url2.GetPort();
}

bool IsPathPrefix(const std::string& p1, const std::string& p2) {
  if (p1.length() > p2.length())
    return false;
  auto mismatches = std::ranges::mismatch(p1, p2);
  // Necessary condition: `p1` is a string prefix of `p2`.
  if (mismatches.in1 != p1.end()) {
    return false;  // E.g.: (`p1` = "/test", `p2` = "/exam") => false.
  }

  // `p1` is string prefix.
  if (mismatches.in2 == p2.end()) {  // Is exact match?
    return true;                     // E.g.: ("/test", "/test") => true.
  }
  // `p1` is strict string prefix, check full match of last path component.
  if (!p1.empty() && *p1.rbegin() == '/')  // Ends in '/'?
    return true;  // E.g.: ("/test/", "/test/stuff") => true.

  // Finally, `p1` does not end in "/": check first extra character in `p2`.
  // E.g.: ("/test", "/test/stuff") => true; ("/test", "/testing") => false.
  return *(mismatches.in2) == '/';
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
  std::string host = url.GetHost();
  if (base::StartsWith(host, "www.", base::CompareCase::SENSITIVE))
    host.assign(host, 4, std::string::npos);
  return host;
}

}  // namespace history
