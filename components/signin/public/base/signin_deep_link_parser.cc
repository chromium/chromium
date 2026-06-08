// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_deep_link_parser.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/signin/public/base/signin_deep_link_payload.h"
#include "components/signin/public/base/signin_switches.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace signin {
namespace {
constexpr std::string_view kEntryPointIdQueryParamName = "entry_point_id";
constexpr std::string_view kEmailQueryParamName = "email";

std::optional<int> TryParseEntryPointIdRawValue(const GURL& deep_link) {
  std::string entry_point_id_str;
  if (net::GetValueForKeyInQuery(deep_link, kEntryPointIdQueryParamName,
                                 &entry_point_id_str)) {
    int parsed_entry_point_id;
    if (base::StringToInt(entry_point_id_str, &parsed_entry_point_id)) {
      return parsed_entry_point_id;
    }
  }
  return std::nullopt;
}

std::optional<ExternalEntryPoint> MapRawEntryPointIdToExternalEntryPoint(
    const std::optional<int> entry_point_id) {
  if (!entry_point_id.has_value()) {
    return std::nullopt;
  }

  // LINT.IfChange(ExternalEntryPoint)
  auto parsed_entry_point =
      static_cast<ExternalEntryPoint>(entry_point_id.value());
  switch (parsed_entry_point) {
    case ExternalEntryPoint::kDesktopDefault:
    case ExternalEntryPoint::kUnknown:
      return parsed_entry_point;
  }
  return ExternalEntryPoint::kUnknown;
  // LINT.ThenChange(
  //   //components/signin/public/base/signin_deep_link_payload.h:ExternalEntryPoint,
  //   //tools/metrics/histograms/metadata/signin/histograms.xml:ExternalEntryPoint
  // )
}

std::optional<std::string> TryParseEmail(const GURL& deep_link) {
  std::string email;
  // TODO(crbug.com/515297363): Validate the email address format.
  if (net::GetValueForKeyInQuery(deep_link, kEmailQueryParamName, &email) &&
      !email.empty()) {
    return email;
  }
  return std::nullopt;
}

}  // namespace

std::optional<SigninDeepLinkPayload> SigninDeepLinkParser::Parse(
    const GURL& deep_link) const {
  if (!deep_link.is_valid()) {
    return std::nullopt;
  }

  if (deep_link.scheme() != expected_base_url_.scheme() ||
      deep_link.host() != expected_base_url_.host() ||
      deep_link.EffectiveIntPort() != expected_base_url_.EffectiveIntPort() ||
      deep_link.path() != expected_base_url_.path()) {
    return std::nullopt;
  }

  std::optional<int> entry_point_id = TryParseEntryPointIdRawValue(deep_link);
  return SigninDeepLinkPayload{
      .entry_point_id = MapRawEntryPointIdToExternalEntryPoint(entry_point_id),
      .entry_point_id_raw_value_for_metrics = entry_point_id,
      .email = TryParseEmail(deep_link),
  };
}

// static
std::optional<SigninDeepLinkParser>
SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled() {
  if (!base::FeatureList::IsEnabled(switches::kCrossDeviceSignin)) {
    return std::nullopt;
  }
  std::string url_str = switches::kCrossDeviceSigninUrl.Get();
  const GURL url(url_str);
  if (!url.is_valid()) {
    return std::nullopt;
  }
  return SigninDeepLinkParser(url);
}

}  // namespace signin
