// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/asset_link_data.h"

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"

namespace password_manager {
namespace {

constexpr char kGetLoginsRelation[] =
    "delegate_permission/common.get_login_creds";
constexpr char kWebNamespace[] = "web";

struct Target {
  std::string target_namespace;
  std::string site;

  static void RegisterJSONConverter(
      base::JSONValueConverter<Target>* converter) {
    converter->RegisterStringField("namespace", &Target::target_namespace);
    converter->RegisterStringField("site", &Target::site);
  }
};

struct Statement {
  std::string include;
  std::vector<std::unique_ptr<std::string>> relations;
  Target target;

  static void RegisterJSONConverter(
      base::JSONValueConverter<Statement>* converter) {
    converter->RegisterStringField("include", &Statement::include);
    converter->RegisterRepeatedString("relation", &Statement::relations);
    converter->RegisterNestedField("target", &Statement::target);
  }
};

}  // namespace

AssetLinkData::AssetLinkData() = default;

AssetLinkData::AssetLinkData(AssetLinkData&& other) noexcept = default;

AssetLinkData::~AssetLinkData() = default;

AssetLinkData& AssetLinkData::operator=(AssetLinkData&& other) = default;

bool AssetLinkData::Parse(const std::string& data) {
  absl::optional<base::Value> value = base::JSONReader::Read(data);
  if (!value || !value->is_list())
    return false;
  base::JSONValueConverter<Statement> converter;
  for (const auto& item : value->GetList()) {
    Statement statement;
    if (converter.Convert(item, &statement)) {
      if (!statement.include.empty()) {
        // The statement is an 'include' statement.
        GURL include(statement.include);
        if (include.is_valid() && include.SchemeIs(url::kHttpsScheme))
          includes_.push_back(std::move(include));
      } else if (base::ranges::any_of(statement.relations, [](const auto& rel) {
                   return *rel == kGetLoginsRelation;
                 })) {
        // 'get_login_creds' statement. Only web HTTPS targets are interesting.
        if (statement.target.target_namespace == kWebNamespace) {
          GURL site(statement.target.site);
          if (site.is_valid() && site.SchemeIs(url::kHttpsScheme))
            targets_.push_back(std::move(site));
        }
      }
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace password_manager
