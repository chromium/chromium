// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_pattern_parser.h"

#include <stddef.h>

#include <string_view>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace {

const char kDomainWildcard[] = "[*.]";
const size_t kDomainWildcardLength = 4;
const char kHostWildcard[] = "*";
const char kPathWildcard[] = "*";
const char kPortWildcard[] = "*";
const char kSchemeWildcard[] = "*";
const char kUrlPathSeparator = '/';
const char kUrlPortSeparator = ':';
const char kUrlPortAndPathSeparator[] = ":/";

}  // namespace

namespace content_settings {

void PatternParser::Parse(std::string_view pattern_spec,
                          ContentSettingsPattern::BuilderInterface* builder) {
  if (pattern_spec == "*") {
    builder->WithSchemeWildcard();
    builder->WithDomainWildcard();
    builder->WithPortWildcard();
    return;
  }

  // Initialize components for the individual patterns parts to empty
  // sub-strings.
  std::string_view scheme_piece;
  std::string_view host_piece;
  std::string_view port_piece;
  std::string_view path_piece;

  std::string_view::size_type start = 0;
  std::string_view::size_type current_pos = 0;

  if (pattern_spec.empty())
    return;

  // Test if a scheme pattern is in the spec.
  const std::string_view standard_scheme_separator(
      url::kStandardSchemeSeparator);
  current_pos = pattern_spec.find(standard_scheme_separator, start);
  if (current_pos != std::string_view::npos) {
    scheme_piece = pattern_spec.substr(start, current_pos - start);
    start = current_pos + standard_scheme_separator.size();
    current_pos = start;
  } else {
    current_pos = start;
  }

  if (start >= pattern_spec.size())
    return;  // Bad pattern spec.

  // Jump to the end of domain wildcards or an IPv6 addresses. IPv6 addresses
  // contain ':'. So first move to the end of an IPv6 address befor searching
  // for the ':' that separates the port form the host.
  if (pattern_spec[current_pos] == '[')
    current_pos = pattern_spec.find("]", start);

  if (current_pos == std::string_view::npos) {
    return;  // Bad pattern spec.
  }

  current_pos =
      pattern_spec.find_first_of(kUrlPortAndPathSeparator, current_pos);
  if (current_pos == std::string_view::npos) {
    // No port spec found AND no path found.
    current_pos = pattern_spec.size();
    host_piece = pattern_spec.substr(start, current_pos - start);
    start = current_pos;
  } else if (pattern_spec[current_pos] == kUrlPathSeparator) {
    // Pattern has a path spec.
    host_piece = pattern_spec.substr(start, current_pos - start);
    start = current_pos;
  } else if (pattern_spec[current_pos] == kUrlPortSeparator) {
    // Port spec found.
    host_piece = pattern_spec.substr(start, current_pos - start);
    start = current_pos + 1;
    if (start < pattern_spec.size()) {
      current_pos = pattern_spec.find(kUrlPathSeparator, start);
      if (current_pos == std::string_view::npos) {
        current_pos = pattern_spec.size();
      }
      port_piece = pattern_spec.substr(start, current_pos - start);
      start = current_pos;
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  current_pos = pattern_spec.size();
  if (start < current_pos) {
    // Pattern has a path spec.
    path_piece = pattern_spec.substr(start, current_pos - start);
  }

  // Set pattern parts.
  if (!scheme_piece.empty()) {
    if (scheme_piece == kSchemeWildcard) {
      builder->WithSchemeWildcard();
    } else {
      builder->WithScheme(std::string(scheme_piece));
    }
  } else {
    builder->WithSchemeWildcard();
  }

  if (!host_piece.empty()) {
    if (host_piece == kHostWildcard) {
      if (ContentSettingsPattern::IsNonWildcardDomainNonPortScheme(
              scheme_piece)) {
        builder->Invalid();
        return;
      }

      builder->WithDomainWildcard();
    } else if (base::StartsWith(host_piece, kDomainWildcard,
                                base::CompareCase::SENSITIVE)) {
      if (ContentSettingsPattern::IsNonWildcardDomainNonPortScheme(
              scheme_piece)) {
        builder->Invalid();
        return;
      }

      host_piece.remove_prefix(kDomainWildcardLength);
      builder->WithDomainWildcard();
      builder->WithHost(std::string(host_piece));
    } else {
      // If the host contains a wildcard symbol then it is invalid.
      if (host_piece.find(kHostWildcard) != std::string_view::npos) {
        builder->Invalid();
        return;
      }
      builder->WithHost(std::string(host_piece));
    }
  }

  bool port_allowed =
      !ContentSettingsPattern::IsNonWildcardDomainNonPortScheme(scheme_piece) &&
      !base::EqualsCaseInsensitiveASCII(scheme_piece, url::kFileScheme);
  if (!port_piece.empty()) {
    if (!port_allowed) {
      builder->Invalid();
      return;
    }

    if (port_piece == kPortWildcard) {
      builder->WithPortWildcard();
    } else {
      // Check if the port string represents a valid port.
      for (const auto port_char : port_piece) {
        if (!base::IsAsciiDigit(port_char)) {
          builder->Invalid();
          return;
        }
      }
      // TODO(markusheintz): Check port range.
      builder->WithPort(std::string(port_piece));
    }
  } else if (port_allowed) {
    builder->WithPortWildcard();
  }

  if (!path_piece.empty()) {
    if (path_piece.substr(1) == kPathWildcard)
      builder->WithPathWildcard();
    else
      builder->WithPath(std::string(path_piece));
  }
}

// static
std::string PatternParser::ToString(
    const ContentSettingsPattern::PatternParts& parts) {
  // Return the most compact form to support legacy code and legacy pattern
  // strings.
  if (parts.is_scheme_wildcard && parts.has_domain_wildcard &&
      parts.host.empty() && parts.is_port_wildcard) {
    return "*";
  }

  std::string str;

  if (!parts.is_scheme_wildcard) {
    str += parts.scheme;
    str += url::kStandardSchemeSeparator;
  }

  if (parts.scheme == url::kFileScheme) {
    if (parts.is_path_wildcard) {
      str += kUrlPathSeparator;
      str += kPathWildcard;
      return str;
    }
    str += parts.path;
    return str;
  }

  if (parts.has_domain_wildcard) {
    if (parts.host.empty())
      str += kHostWildcard;
    else
      str += kDomainWildcard;
  }
  str += parts.host;

  if (ContentSettingsPattern::IsNonWildcardDomainNonPortScheme(parts.scheme)) {
    if (parts.path.empty())
      str += kUrlPathSeparator;
    else
      str += parts.path;
    return str;
  }

  if (!parts.is_port_wildcard) {
    str += kUrlPortSeparator;
    str += parts.port;
  }

  return str;
}

GURL PatternParser::ToRepresentativeUrl(
    const ContentSettingsPattern::PatternParts& parts) {
  if (parts.scheme == url::kFileScheme) {
    if (parts.is_path_wildcard) {
      return GURL();
    }
    return GURL(parts.scheme + url::kStandardSchemeSeparator + parts.path);
  }

  if (parts.host.empty()) {
    return GURL();
  }

  std::string default_port;
  GURL::Replacements r;
  r.SetHostStr(parts.host);

  if (!parts.is_scheme_wildcard) {
    r.SetSchemeStr(parts.scheme);
    default_port =
        base::NumberToString(url::DefaultPortForScheme(parts.scheme));
    r.SetPortStr(default_port);
  }

  if (!parts.is_port_wildcard) {
    r.SetPortStr(parts.port);
  }

  GURL url("https://example.com");
  url = url.ReplaceComponents(r);
  DCHECK(url.is_valid()) << "parts: " << ToString(parts);
  return url;
}

}  // namespace content_settings
