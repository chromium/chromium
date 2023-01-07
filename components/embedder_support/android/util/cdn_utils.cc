// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/cdn_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace embedder_support {

namespace {

constexpr char kDefaultTrustedCDNBaseURL[] = "https://cdn.ampproject.org";

// Specifies a base URL for the trusted CDN for tests.
const char kTrustedCDNBaseURLForTests[] = "trusted-cdn-base-url-for-tests";

// Returns whether the given URL is hosted by a trusted CDN. This can be turned
// off via a Feature, and the base URL to trust can be set via a command line
// flag for testing.
bool IsTrustedCDN(const GURL& url) {
  if (!base::FeatureList::IsEnabled(kShowTrustedPublisherURL))
    return false;

  // Use a static local (without destructor) to construct the base URL only
  // once. |trusted_cdn_base_url| is initialized with the result of an
  // immediately evaluated lambda, which allows wrapping the code in a single
  // expression.
  static const base::NoDestructor<GURL> trusted_cdn_base_url([]() {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kTrustedCDNBaseURLForTests)) {
      GURL base_url(
          command_line->GetSwitchValueASCII(kTrustedCDNBaseURLForTests));
      LOG_IF(WARNING, !base_url.is_valid()) << "Invalid trusted CDN base URL: "
                                            << base_url.possibly_invalid_spec();
      return base_url;
    }

    return GURL(kDefaultTrustedCDNBaseURL);
  }());

  // Allow any subdomain of the base URL.
  return url.DomainIs(trusted_cdn_base_url->host_piece()) &&
         (url.scheme_piece() == trusted_cdn_base_url->scheme_piece()) &&
         (url.EffectiveIntPort() == trusted_cdn_base_url->EffectiveIntPort());
}

}  // namespace

BASE_FEATURE(kShowTrustedPublisherURL,
             "ShowTrustedPublisherURL",
             base::FEATURE_ENABLED_BY_DEFAULT);

GURL GetPublisherURL(content::Page& page) {
  content::RenderFrameHost& rfh = page.GetMainDocument();
  if (!IsTrustedCDN(rfh.GetLastCommittedURL()))
    return GURL();

  const net::HttpResponseHeaders* headers = rfh.GetLastResponseHeaders();
  if (!headers) {
    // TODO(https://crbug.com/829323): In some cases other than offline pages
    // we don't have headers.
    LOG(WARNING) << "No headers for navigation to "
                 << rfh.GetLastCommittedURL();
    return GURL();
  }

  std::string publisher_url;
  if (!headers->GetNormalizedHeader("x-amp-cache", &publisher_url))
    return GURL();

  return GURL(publisher_url);
}

}  // namespace embedder_support
