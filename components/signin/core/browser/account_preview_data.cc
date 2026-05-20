// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data.h"

#include "base/json/json_reader.h"
#include "base/values.h"

namespace signin {

namespace {

constexpr std::string_view kPasswordCountKey = "password_count";
constexpr std::string_view kBookmarkCountKey = "bookmark_count";
constexpr std::string_view kHistoryCountKey = "history_count";
constexpr std::string_view kPasswordDomainsKey = "password_domains";

}  // namespace

AccountPreviewData::AccountPreviewData() = default;
AccountPreviewData::AccountPreviewData(const AccountPreviewData&) = default;
AccountPreviewData::AccountPreviewData(AccountPreviewData&&) noexcept = default;
AccountPreviewData& AccountPreviewData::operator=(const AccountPreviewData&) =
    default;
AccountPreviewData& AccountPreviewData::operator=(
    AccountPreviewData&&) noexcept = default;
AccountPreviewData::~AccountPreviewData() = default;

// static
base::DictValue AccountPreviewData::Serialize(const AccountPreviewData& data) {
  base::DictValue dict;
  dict.Set(kPasswordCountKey, data.password_count);
  dict.Set(kBookmarkCountKey, data.bookmark_count);
  dict.Set(kHistoryCountKey, data.history_count);

  base::ListValue domains;
  for (const std::string& domain : data.password_domains) {
    domains.Append(domain);
  }
  dict.Set(kPasswordDomainsKey, std::move(domains));
  return dict;
}

// static
std::optional<AccountPreviewData> AccountPreviewData::Deserialize(
    std::string_view response_body) {
  std::optional<base::Value> parsed =
      base::JSONReader::Read(response_body, base::JSON_PARSE_RFC);
  if (!parsed) {
    return std::nullopt;
  }
  return Deserialize(*parsed);
}

// static
std::optional<AccountPreviewData> AccountPreviewData::Deserialize(
    const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }
  const auto& dict = value.GetDict();
  AccountPreviewData data;

  // Parse statistics
  if (const base::Value* val = dict.Find(kPasswordCountKey)) {
    if (val->is_int()) {
      data.password_count = val->GetInt();
    }
  }
  if (const base::Value* val = dict.Find(kBookmarkCountKey)) {
    if (val->is_int()) {
      data.bookmark_count = val->GetInt();
    }
  }
  if (const base::Value* val = dict.Find(kHistoryCountKey)) {
    if (val->is_int()) {
      data.history_count = val->GetInt();
    }
  }

  // Parse preview data: list of domain previews
  if (const auto* domains = dict.FindList(kPasswordDomainsKey)) {
    for (const auto& item : *domains) {
      if (item.is_string()) {
        data.password_domains.push_back(item.GetString());
      }
    }
  }

  return data;
}

}  // namespace signin
