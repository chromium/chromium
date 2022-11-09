// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/os_registration.h"

#include <utility>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {

namespace {

bool IsValidUrl(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() &&
         network::IsOriginPotentiallyTrustworthy(url::Origin::Create(url));
}

GURL ParseURLFromStructuredHeaderItem(base::StringPiece header) {
  const auto item = net::structured_headers::ParseItem(header);
  if (!item || !item->item.is_string())
    return GURL();

  GURL url(item->item.GetString());
  return IsValidUrl(url) ? url : GURL();
}

}  // namespace

// static
absl::optional<OsSource> OsSource::Parse(base::StringPiece header) {
  GURL url = ParseURLFromStructuredHeaderItem(header);
  if (!url.is_valid())
    return absl::nullopt;

  return OsSource(std::move(url));
}

// static
absl::optional<OsSource> OsSource::Create(GURL url) {
  if (!IsValidUrl(url))
    return absl::nullopt;

  return OsSource(std::move(url));
}

// static
absl::optional<OsTrigger> OsTrigger::Parse(base::StringPiece header) {
  GURL url = ParseURLFromStructuredHeaderItem(header);
  if (!url.is_valid())
    return absl::nullopt;

  return OsTrigger(std::move(url));
}

// static
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

OsSource::OsSource(GURL url) : url_(std::move(url)) {
  DCHECK(IsValidUrl(url_));
}

OsSource::~OsSource() = default;

OsSource::OsSource(const OsSource&) = default;

OsSource& OsSource::operator=(const OsSource&) = default;

OsSource::OsSource(OsSource&&) = default;

OsSource& OsSource::operator=(OsSource&&) = default;

}  // namespace attribution_reporting
