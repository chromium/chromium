// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_info.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/attribution_reporting/registrar.h"
#include "net/http/structured_headers.h"

namespace attribution_reporting {

namespace {

base::expected<std::optional<Registrar>, RegistrationInfoError>
ParsePreferredPlatform(const net::structured_headers::Dictionary& dict,
                       bool cross_app_web_enabled) {
  if (!cross_app_web_enabled) {
    return std::nullopt;
  }

  auto iter = dict.find("preferred-platform");
  if (iter == dict.end()) {
    return std::nullopt;
  }

  const auto& parameterized_member = iter->second;
  if (parameterized_member.member_is_inner_list) {
    return base::unexpected(RegistrationInfoError());
  }

  const auto& parameterized_item = parameterized_member.member.front();
  if (!parameterized_item.item.is_token()) {
    return base::unexpected(RegistrationInfoError());
  }

  const std::string& token = parameterized_item.item.GetString();
  if (token == "web") {
    return Registrar::kWeb;
  } else if (token == "os") {
    return Registrar::kOs;
  } else {
    return base::unexpected(RegistrationInfoError());
  }
}

base::expected<bool, RegistrationInfoError> ParseReportHeaderErrors(
    const net::structured_headers::Dictionary& dict) {
  auto iter = dict.find("report-header-errors");
  if (iter == dict.end()) {
    return false;
  }

  const auto& parameterized_member = iter->second;
  if (parameterized_member.member_is_inner_list) {
    return base::unexpected(RegistrationInfoError());
  }

  const auto& parameterized_item = parameterized_member.member.front();
  if (!parameterized_item.item.is_boolean()) {
    return base::unexpected(RegistrationInfoError());
  }

  return parameterized_item.item.GetBoolean();
}

}  // namespace

// static
base::expected<RegistrationInfo, RegistrationInfoError>
RegistrationInfo::ParseInfo(std::string_view header,
                            bool cross_app_web_enabled) {
  if (header.empty()) {
    return RegistrationInfo();
  }

  const auto dict = net::structured_headers::ParseDictionary(header);
  if (!dict) {
    return base::unexpected(RegistrationInfoError());
  }

  return ParseInfo(*dict, cross_app_web_enabled);
}

// static
base::expected<RegistrationInfo, RegistrationInfoError>
RegistrationInfo::ParseInfo(const net::structured_headers::Dictionary& dict,
                            bool cross_app_web_enabled) {
  ASSIGN_OR_RETURN(std::optional<Registrar> preferred_platform,
                   ParsePreferredPlatform(dict, cross_app_web_enabled));

  ASSIGN_OR_RETURN(bool report_header_errors, ParseReportHeaderErrors(dict));

  return RegistrationInfo(preferred_platform, report_header_errors);
}

}  // namespace attribution_reporting
