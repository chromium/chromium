// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/default_chrome_apps_migrator.h"

#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

std::map<std::string, std::string> GetChromeAppToWebAppMapping() {
  return std::map<std::string, std::string>({
      // Maps
      {"lneaknkopdijkpnocmklfnjbeapigfbh",
       "https://www.google.com/maps?force=tt&source=ttpwa"},
  });
}

}  // namespace

DefaultChromeAppsMigrator::DefaultChromeAppsMigrator()
    : DefaultChromeAppsMigrator(GetChromeAppToWebAppMapping()) {}

DefaultChromeAppsMigrator::DefaultChromeAppsMigrator(
    std::map<std::string, std::string> chrome_app_to_web_app)
    : chrome_app_to_web_app_(std::move(chrome_app_to_web_app)) {}

DefaultChromeAppsMigrator::DefaultChromeAppsMigrator(
    DefaultChromeAppsMigrator&&) noexcept = default;
DefaultChromeAppsMigrator& DefaultChromeAppsMigrator::operator=(
    DefaultChromeAppsMigrator&&) noexcept = default;

DefaultChromeAppsMigrator::~DefaultChromeAppsMigrator() = default;

void DefaultChromeAppsMigrator::Migrate(PolicyMap* policies) const {
  PolicyMap::Entry* forcelist_entry =
      policies->GetMutable(key::kExtensionInstallForcelist);
  if (!forcelist_entry)
    return;

  const base::Value* forcelist_value = forcelist_entry->value();
  if (!forcelist_value || !forcelist_value->is_list())
    return;

  base::Value new_forcelist_value(base::Value::Type::LIST);
  std::vector<std::string> chrome_app_ids;
  std::vector<std::string> web_app_urls;
  // Remove Chrome apps listed in `chrome_app_to_web_app_` from new
  // ExtensionInstallForcelist value. Add the Chrome app ids that need to be
  // blocked to 'chrome_app_ids'. Add the URLs of Web Apps that need to be
  // installed to `web_app_urls`.
  for (const auto& list_entry : forcelist_value->GetList()) {
    if (!list_entry.is_string()) {
      new_forcelist_value.Append(list_entry.Clone());
      continue;
    }

    const std::string entry = list_entry.GetString();
    const size_t pos = entry.find(';');
    const std::string extension_id = entry.substr(0, pos);

    const auto iter = chrome_app_to_web_app_.find(extension_id);
    if (iter != chrome_app_to_web_app_.end()) {
      chrome_app_ids.push_back(iter->first);
      web_app_urls.push_back(iter->second);
    } else {
      new_forcelist_value.Append(entry);
    }
  }

  // If no chrome apps need to be replaced, we have nothing to do.
  if (chrome_app_ids.empty())
    return;

  forcelist_entry->set_value(std::move(new_forcelist_value));

  EnsurePolicyValueIsList(policies, key::kExtensionInstallBlocklist);
  base::Value* blocklist_value =
      policies->GetMutableValue(key::kExtensionInstallBlocklist);
  for (const std::string& chrome_app_id : chrome_app_ids) {
    blocklist_value->Append(chrome_app_id);
  }

  EnsurePolicyValueIsList(policies, key::kWebAppInstallForceList);
  base::Value* web_app_policy_value =
      policies->GetMutableValue(key::kWebAppInstallForceList);
  for (const std::string& web_app_url : web_app_urls) {
    base::Value web_app(base::Value::Type::DICTIONARY);
    web_app.SetStringKey("url", web_app_url);
    web_app_policy_value->Append(std::move(web_app));
  }
}

void DefaultChromeAppsMigrator::EnsurePolicyValueIsList(
    PolicyMap* policies,
    const std::string& policy_name) const {
  const base::Value* policy_value = policies->GetValue(policy_name);
  if (!policy_value || !policy_value->is_list()) {
    const PolicyMap::Entry* forcelist_entry =
        policies->Get(key::kExtensionInstallForcelist);
    PolicyMap::Entry policy_entry(
        forcelist_entry->level, forcelist_entry->scope, forcelist_entry->source,
        base::Value(base::Value::Type::LIST), nullptr);
    // If `policy_value` has wrong type, add message before overriding value.
    if (policy_value) {
      policy_entry.AddMessage(PolicyMap::MessageType::kError,
                              IDS_POLICY_TYPE_ERROR);
    }
    policies->Set(policy_name, std::move(policy_entry));
  }
}

}  // namespace policy