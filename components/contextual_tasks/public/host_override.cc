// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/host_override.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/base/url_util.h"
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
      return HostOverride{std::string(str), std::nullopt};
    }
  }

  std::string parsed_host;
  int parsed_port = -1;
  if (!net::ParseHostAndPort(str, &parsed_host, &parsed_port)) {
    return std::nullopt;
  }

  if (parsed_host.empty()) {
    return std::nullopt;
  }

  std::optional<uint16_t> port;
  if (parsed_port != -1) {
    port = static_cast<uint16_t>(parsed_port);
  }

  return HostOverride{std::move(parsed_host), port};
}

std::string HostOverride::ToString() const {
  std::string formatted_host = (host.find(':') != std::string::npos &&
                                !host.empty() && host.front() != '[')
                                   ? base::StrCat({"[", host, "]"})
                                   : host;
  if (port.has_value()) {
    return base::StrCat({formatted_host, ":", base::NumberToString(*port)});
  }
  return formatted_host;
}

bool HostOverride::Matches(const GURL& url) const {
  if (!base::EqualsCaseInsensitiveASCII(host, url.host()) &&
      !base::EqualsCaseInsensitiveASCII(host, url.HostNoBracketsPiece())) {
    return false;
  }
  return port ? (url.EffectiveIntPort() == *port) : !url.has_port();
}

GURL HostOverride::ApplyToUrl(const GURL& url) const {
  GURL::Replacements replacements;
  std::string formatted_host = (host.find(':') != std::string::npos &&
                                !host.empty() && host.front() != '[')
                                   ? base::StrCat({"[", host, "]"})
                                   : host;
  replacements.SetHostStr(formatted_host);
  std::string port_str;
  if (port) {
    port_str = base::NumberToString(*port);
    replacements.SetPortStr(port_str);
  } else if (url.has_port()) {
    replacements.ClearPort();
  }
  return url.ReplaceComponents(replacements);
}

}  // namespace contextual_tasks
