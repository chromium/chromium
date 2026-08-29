// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/host_override.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/url_canon_ip.h"

namespace contextual_tasks {

std::optional<HostOverride> HostOverride::FromString(std::string_view str) {
  if (str.empty()) {
    return std::nullopt;
  }

  // Check for unbracketed IPv6 literal (e.g. "::1").
  if (str.find(':') != std::string_view::npos && str.front() != '[') {
    std::string bracketed = base::StrCat({"[", str, "]"});
    unsigned char tmp_ipv6[16];
    if (url::IPv6AddressToNumber(bracketed, tmp_ipv6)) {
      return HostOverride{std::string(str)};
    }
  }

  // Strip brackets if bracketed IPv6.
  if (str.size() >= 2 && str.front() == '[' && str.back() == ']') {
    return HostOverride{std::string(str.substr(1, str.size() - 2))};
  }

  return HostOverride{std::string(str)};
}

std::string HostOverride::ToString() const {
  if (host.find(':') != std::string::npos && !host.empty() &&
      host.front() != '[') {
    return base::StrCat({"[", host, "]"});
  }
  return host;
}

bool HostOverride::Matches(const GURL& url) const {
  return base::EqualsCaseInsensitiveASCII(host, url.host()) ||
         base::EqualsCaseInsensitiveASCII(host, url.HostNoBracketsPiece());
}

GURL HostOverride::ApplyToUrl(const GURL& url) const {
  GURL::Replacements replacements;
  std::string formatted_host = (host.find(':') != std::string::npos &&
                                !host.empty() && host.front() != '[')
                                   ? base::StrCat({"[", host, "]"})
                                   : host;
  replacements.SetHostStr(formatted_host);
  return url.ReplaceComponents(replacements);
}

}  // namespace contextual_tasks
