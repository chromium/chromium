// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parse.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace attribution_reporting {

namespace {

bool IsValidUrl(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() &&
         network::IsOriginPotentiallyTrustworthy(url::Origin::Create(url));
}

bool IsValidWebDestination(const url::Origin& origin) {
  if (origin.scheme() != url::kHttpScheme &&
      origin.scheme() != url::kHttpsScheme) {
    return false;
  }

  return network::IsOriginPotentiallyTrustworthy(origin);
}

}  // namespace

// static
absl::optional<OsSource> OsSource::Parse(base::StringPiece header) {
  const auto item = net::structured_headers::ParseItem(header);

  if (!item || !item->item.is_string())
    return absl::nullopt;

  GURL url(item->item.GetString());

  if (!IsValidUrl(url))
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

      web_destination = url::Origin::Create(GURL(it.second.GetString()));

      if (!IsValidWebDestination(web_destination))
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

absl::optional<OsSource> OsSource::Create(GURL url,
                                          std::string os_destination,
                                          url::Origin web_destination) {
  if (!IsValidUrl(url) || !IsValidWebDestination(web_destination))
    return absl::nullopt;

  return OsSource(std::move(url), std::move(os_destination),
                  std::move(web_destination));
}

absl::optional<OsTrigger> OsTrigger::Parse(base::StringPiece header) {
  const auto item = net::structured_headers::ParseItem(header);

  if (!item || !item->item.is_string())
    return absl::nullopt;

  return Create(GURL(item->item.GetString()));
}

absl::optional<OsTrigger> OsTrigger::Create(GURL url) {
  if (!IsValidUrl(url))
    return absl::nullopt;

  return OsTrigger(std::move(url));
}

OsTrigger::OsTrigger() = default;

OsTrigger::OsTrigger(GURL url) : url_(std::move(url)) {
  DCHECK(IsValidUrl(url_));
}

OsTrigger::~OsTrigger() = default;

OsTrigger::OsTrigger(const OsTrigger&) = default;

OsTrigger& OsTrigger::operator=(const OsTrigger&) = default;

OsTrigger::OsTrigger(OsTrigger&&) = default;

OsTrigger& OsTrigger::operator=(OsTrigger&&) = default;

OsSource::OsSource() = default;

OsSource::OsSource(GURL url,
                   std::string os_destination,
                   url::Origin web_destination)
    : url_(std::move(url)),
      os_destination_(std::move(os_destination)),
      web_destination_(std::move(web_destination)) {
  DCHECK(IsValidUrl(url_));
  DCHECK(IsValidWebDestination(web_destination_));
}

OsSource::~OsSource() = default;

OsSource::OsSource(const OsSource&) = default;

OsSource& OsSource::operator=(const OsSource&) = default;

OsSource::OsSource(OsSource&&) = default;

OsSource& OsSource::operator=(OsSource&&) = default;

}  // namespace attribution_reporting
