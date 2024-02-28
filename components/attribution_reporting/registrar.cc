// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registrar.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/types/expected.h"
#include "net/http/structured_headers.h"

namespace attribution_reporting {

namespace {
constexpr char kPreferredPlatform[] = "preferred-platform";
}  // namespace

base::expected<std::optional<Registrar>, PreferredPlatformError> ParseInfo(
    std::string_view header) {
  if (header.empty()) {
    return std::nullopt;
  }

  const auto dict = net::structured_headers::ParseDictionary(header);
  if (!dict) {
    return base::unexpected(PreferredPlatformError());
  }

  auto iter = dict->find(kPreferredPlatform);
  if (iter == dict->end()) {
    return std::nullopt;
  }

  const auto& parameterized_member = iter->second;
  if (parameterized_member.member_is_inner_list) {
    return base::unexpected(PreferredPlatformError());
  }

  const auto& parameterized_item = parameterized_member.member.front();
  if (!parameterized_item.item.is_token()) {
    return base::unexpected(PreferredPlatformError());
  }

  const std::string& token = parameterized_item.item.GetString();
  if (token == "web") {
    return Registrar::kWeb;
  } else if (token == "os") {
    return Registrar::kOs;
  } else {
    return base::unexpected(PreferredPlatformError());
  }
}

}  // namespace attribution_reporting
