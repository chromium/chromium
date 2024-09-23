// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/elide_url.h"

#include <stddef.h>

#include <string_view>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/robolectric_buildflags.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_ROBOLECTRIC)
#include "ui/gfx/text_constants.h"  // nogncheck
#include "ui/gfx/text_elider.h"     // nogncheck
#include "ui/gfx/text_utils.h"      // nogncheck
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_ROBOLECTRIC)
const char16_t kDot = '.';

// Build a path from the first |num_components| elements in |path_elements|.
// Prepends |path_prefix|, appends |filename|, inserts ellipsis if appropriate.
std::u16string BuildPathFromComponents(
    const std::u16string& path_prefix,
    const std::vector<std::u16string>& path_elements,
    const std::u16string& filename,
    size_t num_components) {
  DCHECK_LE(num_components, path_elements.size());

  // Add the initial elements of the path.
  std::u16string path = path_prefix;

  // Build path from first |num_components| elements.
  for (size_t j = 0; j < num_components; ++j)
    path += path_elements[j] + gfx::kForwardSlash;

  // Add |filename|, ellipsis if necessary.
  if (num_components != (path_elements.size() - 1))
    path += std::u16string(gfx::kEllipsisUTF16) + gfx::kForwardSlash;
  path += filename;

  return path;
}

// Takes a prefix (Domain, or Domain+subdomain) and a collection of path
// components and elides if possible. Returns a string containing the longest
// possible elided path, or an empty string if elision is not possible.
std::u16string ElideComponentizedPath(
    const std::u16string& url_path_prefix,
    const std::vector<std::u16string>& url_path_elements,
    const std::u16string& url_filename,
    const std::u16string& url_query,
    const gfx::FontList& font_list,
    float available_pixel_width) {
  CHECK(!url_path_elements.empty());

  // Find the longest set of leading path components that fits in
  // |available_pixel_width|.  Since BuildPathFromComponents() is O(n), using a
  // binary search here makes the overall complexity O(n lg n), which is
  // meaningful since there may be thousands of components in extreme cases.
  std::u16string elided_path_at_min_index;
  size_t min_index = 0;
  for (size_t max_index = url_path_elements.size();
       min_index != max_index - 1;) {
    const size_t cutting_index = (min_index + max_index) / 2;
    const std::u16string elided_path = BuildPathFromComponents(
        url_path_prefix, url_path_elements, url_filename, cutting_index);
    if (gfx::GetStringWidthF(elided_path, font_list) <= available_pixel_width) {
      min_index = cutting_index;
      elided_path_at_min_index = elided_path;
    } else {
      max_index = cutting_index;
    }
  }

  // If the cutting point is at the beginning and nothing gets elided, return
  // failure even if the whole text could fit. TODO(crbug.com/40127834).
  if (min_index == 0)
    return std::u16string();

  // Elide starting at |min_index|.
  return gfx::ElideText(elided_path_at_min_index + url_query, font_list,
                        available_pixel_width, gfx::ELIDE_TAIL);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_ROBOLECTRIC)

bool ShouldShowScheme(std::string_view scheme,
                      const url_formatter::SchemeDisplay scheme_display) {
  switch (scheme_display) {
    case url_formatter::SchemeDisplay::SHOW:
      return true;

    case url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS:
      return scheme != url::kHttpsScheme && scheme != url::kHttpScheme;

    case url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC:
      return scheme != url::kHttpsScheme && scheme != url::kWssScheme;
  }

  return true;
}

// TODO(jshin): Come up with a way to show Bidi URLs 'safely' (e.g. wrap up
// the entire url with {LSI, PDI} and individual domain labels with {FSI, PDI}).
// See http://crbug.com/650760 . For now, fall back to punycode if there's a
// strong RTL character.
std::u16string HostForDisplay(std::string_view host_in_puny) {
  std::u16string host = url_formatter::IDNToUnicode(host_in_puny);
  return base::i18n::StringContainsStrongRTLChars(host) ?
      base::ASCIIToUTF16(host_in_puny) : host;
}

}  // namespace

namespace url_formatter {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_ROBOLECTRIC)

// TODO(pkasting): http://crbug.com/77883 This whole function gets
// kerning/ligatures/etc. issues potentially wrong by assuming that the width of
// a rendered string is always the sum of the widths of its substrings.  Also I
// suspect it could be made simpler.
std::u16string ElideUrl(const GURL& url,
                        const gfx::FontList& font_list,
                        float available_pixel_width) {
  // Get a formatted string and corresponding parsing of the url.
  url::Parsed parsed;
  const std::u16string url_string = url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitDefaults, base::UnescapeRule::SPACES,
      &parsed, nullptr, nullptr);
  if (available_pixel_width <= 0)
    return url_string;

  if (!url.IsStandard()) {
    return gfx::ElideText(url_string, font_list, available_pixel_width,
                          gfx::ELIDE_TAIL);
  }

  // Now start eliding url_string to fit within available pixel width.
  // Fist pass - check to see whether entire url_string fits.
  const float pixel_width_url_string =
      gfx::GetStringWidthF(url_string, font_list);
  if (available_pixel_width >= pixel_width_url_string)
    return url_string;

  // Get the path substring, including query and reference.
  const size_t path_start_index = parsed.path.begin;
  const size_t path_len = parsed.path.len;
  std::u16string url_path_query_etc;
  std::u16string url_path;
  if (parsed.path.is_valid()) {
    url_path_query_etc = url_string.substr(path_start_index);
    url_path = url_string.substr(path_start_index, path_len);
  }

  // Return general elided text if url minus the query fits.
  const std::u16string url_minus_query =
      url_string.substr(0, path_start_index + path_len);
  if (available_pixel_width >=
      gfx::GetStringWidthF(url_minus_query, font_list)) {
    return gfx::ElideText(url_string, font_list, available_pixel_width,
                          gfx::ELIDE_TAIL);
  }

  std::u16string url_host;
  std::u16string url_domain;
  std::u16string url_subdomain;
  url_formatter::SplitHost(url, &url_host, &url_domain, &url_subdomain);

  // If this is a file type, the path is now defined as everything after ":".
  // For example, "C:/aa/aa/bb", the path is "/aa/bb/cc". Interesting, the
  // domain is now C: - this is a nice hack for eliding to work pleasantly.
  if (url.SchemeIsFile()) {
    // Split the path string using ":"
    constexpr std::u16string_view kColon(u":", 1);
    std::vector<std::u16string> file_path_split = base::SplitString(
        url_path, kColon, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (file_path_split.size() > 1) {  // File is of type "file:///C:/.."
      url_host.clear();
      url_domain.clear();
      url_subdomain.clear();

      url_host = url_domain =
          base::StrCat({file_path_split.at(0).substr(1), kColon});
      url_path_query_etc = url_path = file_path_split.at(1);
    }
  }

  // Second Pass - remove scheme - the rest fits.
  const float pixel_width_url_host = gfx::GetStringWidthF(url_host, font_list);
  const float pixel_width_url_path =
      gfx::GetStringWidthF(url_path_query_etc, font_list);
  if (available_pixel_width >= pixel_width_url_host + pixel_width_url_path)
    return url_host + url_path_query_etc;

  // Third Pass: Subdomain, domain and entire path fits.
  const float pixel_width_url_domain =
      gfx::GetStringWidthF(url_domain, font_list);
  const float pixel_width_url_subdomain =
      gfx::GetStringWidthF(url_subdomain, font_list);
  if (available_pixel_width >=
      pixel_width_url_subdomain + pixel_width_url_domain + pixel_width_url_path)
    return url_subdomain + url_domain + url_path_query_etc;

  // Query element.
  std::u16string url_query;
  const float kPixelWidthDotsTrailer =
      gfx::GetStringWidthF(std::u16string(gfx::kEllipsisUTF16), font_list);
  if (parsed.query.is_nonempty()) {
    url_query = u"?" + url_string.substr(parsed.query.begin);
    if (available_pixel_width >=
        (pixel_width_url_subdomain + pixel_width_url_domain +
         pixel_width_url_path - gfx::GetStringWidthF(url_query, font_list))) {
      return gfx::ElideText(url_subdomain + url_domain + url_path_query_etc,
                            font_list, available_pixel_width, gfx::ELIDE_TAIL);
    }
  }

  // Parse url_path using '/'.
  std::vector<std::u16string> url_path_elements =
      base::SplitString(url_path, std::u16string(1, gfx::kForwardSlash),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Get filename - note that for a path ending with /
  // such as www.google.com/intl/ads/, the file name is ads/.
  std::u16string url_filename(
      url_path_elements.empty() ? std::u16string() : url_path_elements.back());
  size_t url_path_number_of_elements = url_path_elements.size();
  if (url_filename.empty() && (url_path_number_of_elements > 1)) {
    // Path ends with a '/'.
    --url_path_number_of_elements;
    url_filename =
        url_path_elements[url_path_number_of_elements - 1] + gfx::kForwardSlash;
  }

  // Start eliding the path and replacing elements by ".../".
  const std::u16string kEllipsisAndSlash =
      std::u16string(gfx::kEllipsisUTF16) + gfx::kForwardSlash;
  const float pixel_width_ellipsis_slash =
      gfx::GetStringWidthF(kEllipsisAndSlash, font_list);

  // Check with both subdomain and domain.
  if (url_path_number_of_elements > 0) {
    std::u16string elided_path = ElideComponentizedPath(
        url_subdomain + url_domain, url_path_elements, url_filename, url_query,
        font_list, available_pixel_width);
    if (!elided_path.empty())
      return elided_path;
  }

  // Check with only domain.
  // If a subdomain is present, add an ellipsis before domain.
  // This is added only if the subdomain pixel width is larger than
  // the pixel width of kEllipsis. Otherwise, subdomain remains,
  // which means that this case has been resolved earlier.
  std::u16string url_elided_domain = url_subdomain + url_domain;
  if (pixel_width_url_subdomain > kPixelWidthDotsTrailer) {
    if (!url_subdomain.empty())
      url_elided_domain = kEllipsisAndSlash[0] + url_domain;
    else
      url_elided_domain = url_domain;

    if (url_path_number_of_elements > 0) {
      std::u16string elided_path = ElideComponentizedPath(
          url_elided_domain, url_path_elements, url_filename, url_query,
          font_list, available_pixel_width);
      if (!elided_path.empty())
        return elided_path;
    }
  }

  // Return elided domain/.../filename anyway.
  std::u16string final_elided_url_string(url_elided_domain);
  const float url_elided_domain_width =
      gfx::GetStringWidthF(url_elided_domain, font_list);

  // A hack to prevent trailing ".../...".
  if (url_path_number_of_elements > 0 &&
      url_elided_domain_width + pixel_width_ellipsis_slash +
              kPixelWidthDotsTrailer + gfx::GetStringWidthF(u"UV", font_list) <
          available_pixel_width) {
    final_elided_url_string += BuildPathFromComponents(
        std::u16string(), url_path_elements, url_filename, 1);
  } else {
    final_elided_url_string += url_path;
  }

  return gfx::ElideText(final_elided_url_string, font_list,
                        available_pixel_width, gfx::ELIDE_TAIL);
}

std::u16string ElideHost(const GURL& url,
                         const gfx::FontList& font_list,
                         float available_pixel_width) {
  std::u16string url_host;
  std::u16string url_domain;
  std::u16string url_subdomain;
  url_formatter::SplitHost(url, &url_host, &url_domain, &url_subdomain);

  const float pixel_width_url_host = gfx::GetStringWidthF(url_host, font_list);
  if (available_pixel_width >= pixel_width_url_host)
    return url_host;

  if (url_subdomain.empty())
    return url_domain;

  const float pixel_width_url_domain =
      gfx::GetStringWidthF(url_domain, font_list);
  float subdomain_width = available_pixel_width - pixel_width_url_domain;
  if (subdomain_width <= 0)
    return std::u16string(gfx::kEllipsisUTF16) + kDot + url_domain;

  return gfx::ElideText(url_host, font_list, available_pixel_width,
                        gfx::ELIDE_HEAD);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_ROBOLECTRIC)

std::u16string FormatUrlForSecurityDisplay(const GURL& url,
                                           const SchemeDisplay scheme_display) {
  if (!url.is_valid() || url.is_empty() || !url.IsStandard())
    return url_formatter::FormatUrl(url);

  constexpr std::u16string_view colon(u":");

  if (url.SchemeIsFile()) {
    return base::StrCat({url::kFileScheme16, url::kStandardSchemeSeparator16,
                         base::UTF8ToUTF16(url.path())});
  }

  if (url.SchemeIsFileSystem()) {
    const GURL* inner_url = url.inner_url();
    if (inner_url->SchemeIsFile()) {
      return base::StrCat({url::kFileSystemScheme16, colon,
                           FormatUrlForSecurityDisplay(*inner_url),
                           base::UTF8ToUTF16(url.path())});
    }
    return base::StrCat({url::kFileSystemScheme16, colon,
                         FormatUrlForSecurityDisplay(*inner_url)});
  }

  const GURL origin = url.DeprecatedGetOriginAsURL();
  std::string_view scheme = origin.scheme_piece();
  std::string_view host = origin.host_piece();

  std::u16string result;
  if (ShouldShowScheme(scheme, scheme_display)) {
    result = base::StrCat(
        {base::UTF8ToUTF16(scheme), url::kStandardSchemeSeparator16});
  }
  result += HostForDisplay(host);

  const int port = origin.IntPort();
  const int default_port = url::DefaultPortForScheme(scheme);
  if (port != url::PORT_UNSPECIFIED && port != default_port)
    result += base::StrCat({colon, base::UTF8ToUTF16(origin.port_piece())});

  return result;
}

std::u16string FormatOriginForSecurityDisplay(
    const url::Origin& origin,
    const SchemeDisplay scheme_display) {
  std::string_view scheme = origin.scheme();
  std::string_view host = origin.host();
  if (scheme.empty() && host.empty())
    return std::u16string();

  constexpr std::u16string_view colon(u":");

  std::u16string result;
  if (ShouldShowScheme(scheme, scheme_display)) {
    result = base::StrCat(
        {base::UTF8ToUTF16(scheme), url::kStandardSchemeSeparator16});
  }
  result += HostForDisplay(host);

  int port = static_cast<int>(origin.port());
  const int default_port = url::DefaultPortForScheme(scheme);
  if (port != 0 && port != default_port)
    result += base::StrCat({colon, base::NumberToString16(origin.port())});

  return result;
}

std::u16string FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
    const GURL& url) {
  return url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlTrimAfterHost |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
}

#if BUILDFLAG(IS_IOS)
std::u16string
FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
    const GURL& url) {
  return url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlTrimAfterHost |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlOmitMobilePrefix,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
}
#endif

void SplitHost(const GURL& url,
               std::u16string* url_host,
               std::u16string* url_domain,
               std::u16string* url_subdomain) {
  // GURL stores IDN hostnames in punycode.  Convert back to Unicode for
  // display to the user.  (IDNToUnicode() will only perform this conversion
  // if it's safe to display this host/domain in Unicode.)
  *url_host = url_formatter::IDNToUnicode(url.host());

  // Get domain and registry information from the URL.
  std::string domain_puny =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  *url_domain = domain_puny.empty() ? *url_host
                                    : url_formatter::IDNToUnicode(domain_puny);

  // Add port if required.
  if (!url.port().empty()) {
    *url_host += base::UTF8ToUTF16(":" + url.port());
    *url_domain += base::UTF8ToUTF16(":" + url.port());
  }

  // Get sub domain if requested.
  if (url_subdomain) {
    const size_t domain_start_index = url_host->find(*url_domain);
    constexpr std::u16string_view kWwwPrefix = u"www.";
    if (domain_start_index != std::u16string::npos)
      *url_subdomain = url_host->substr(0, domain_start_index);
    if ((*url_subdomain == kWwwPrefix || url_subdomain->empty() ||
         url.SchemeIsFile())) {
      url_subdomain->clear();
    }
  }
}

}  // namespace url_formatter
