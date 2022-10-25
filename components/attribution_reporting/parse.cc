// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parse.h"

#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {

namespace {

url::Origin OriginIfHttpAndPotentiallyTrustworthy(const GURL& url) {
  url::Origin url_origin = url::Origin::Create(url);
  if (!url.SchemeIsHTTPOrHTTPS())
    return url::Origin();

  auto origin = url::Origin::Create(url);
  return network::IsOriginPotentiallyTrustworthy(origin) ? origin
                                                         : url::Origin();
}

}  // namespace

// static
absl::optional<OsSource> OsSource::Parse(base::StringPiece header) {
  const auto item = net::structured_headers::ParseItem(header);

  if (!item || !item->item.is_string())
    return absl::nullopt;

  GURL url(item->item.GetString());

  if (OriginIfHttpAndPotentiallyTrustworthy(url).opaque())
    return absl::nullopt;

  std::string os_destination;
  url::Origin web_destination;

  bool has_os = false;
  bool has_web = false;

  for (const auto& it : item->params) {
    if (it.first == "os-destination") {
      if (!it.second.is_string())
        return absl::nullopt;

      os_destination = it.second.GetString();
      has_os = true;
    } else if (it.first == "web-destination") {
      if (!it.second.is_string())
        return absl::nullopt;

      web_destination =
          OriginIfHttpAndPotentiallyTrustworthy(GURL(it.second.GetString()));

      if (web_destination.opaque())
        return absl::nullopt;

      has_web = true;
    }
  }

  // A valid header must specify both destinations.
  if (!has_os || !has_web)
    return absl::nullopt;

  return OsSource(std::move(url), std::move(os_destination),
                  std::move(web_destination));
}

// static
OsSource OsSource::CreateForTesting(GURL url,
                                    std::string os_destination,
                                    url::Origin web_destination) {
  return OsSource(std::move(url), std::move(os_destination),
                  std::move(web_destination));
}

GURL ParseOsTriggerRegistrationHeader(base::StringPiece header) {
  const auto item = net::structured_headers::ParseItem(header);

  if (!item || !item->item.is_string())
    return GURL();

  GURL url(item->item.GetString());

  if (OriginIfHttpAndPotentiallyTrustworthy(url).opaque())
    return GURL();

  return url;
}

OsSource::OsSource(GURL url,
                   std::string os_destination,
                   url::Origin web_destination)
    : url_(std::move(url)),
      os_destination_(std::move(os_destination)),
      web_destination_(std::move(web_destination)) {}

OsSource::~OsSource() = default;

OsSource::OsSource(const OsSource&) = default;

OsSource& OsSource::operator=(const OsSource&) = default;

OsSource::OsSource(OsSource&&) = default;

OsSource& OsSource::operator=(OsSource&&) = default;

}  // namespace attribution_reporting
