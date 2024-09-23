// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/default_chrome_apps_migrator.h"

#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

std::map<std::string, std::string> GetChromeAppToWebAppMapping() {
  return std::map<std::string, std::string>({
      {"ejjicmeblgpmajnghnpcppodonldlgfn",
       "https://calendar.google.com/calendar/installwebapp?usp=admin"},
      {"aohghmighlieiainnegkcijnfilokake",
       "https://docs.google.com/document/installwebapp?usp=admin"},
      {"apdfllckaahabafndbhieahigkjlhalf",
       "https://drive.google.com/drive/installwebapp?usp=admin"},
      {"pjkljhegncpnkpknbcohdijeoejaedia",
       "https://mail.google.com/mail/installwebapp?usp=admin"},
      {"felcaaldnbdncclmgdcncolpebgiejap",
       "https://docs.google.com/spreadsheets/installwebapp?usp=admin"},
      {"aapocclcgogkmnckokdopfmhonfmgoek",
       "https://docs.google.com/presentation/installwebapp?usp=admin"},
      {"blpcfgokakmgnkcojhhkbfbldkacnbeo",
       "https://www.youtube.com/s/notifications/manifest/cr_install.html"},
      {"hmjkmjkepdijhoojdojkdfohbdgmmhki",
       "https://keep.google.com/installwebapp?usp=admin"},
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
  std::vector<std::string> chrome_app_ids =
      RemoveChromeAppsFromExtensionForcelist(policies);

  // If no Chrome Apps need to be replaced, we have nothing to do.
  if (chrome_app_ids.empty())
    return;

  EnsurePolicyValueIsList(policies, key::kWebAppInstallForceList);
  base::Value::List& web_app_policy_value =
      policies
          ->GetMutableValue(key::kWebAppInstallForceList,
                            base::Value::Type::LIST)
          ->GetList();
  for (const std::string& chrome_app_id : chrome_app_ids) {
    base::Value::Dict web_app;
    web_app.Set("url", chrome_app_to_web_app_.at(chrome_app_id));
    base::Value::List uninstall_list;
    uninstall_list.Append(chrome_app_id);
    web_app.Set("uninstall_and_replace", std::move(uninstall_list));
    web_app_policy_value.Append(std::move(web_app));
  }

  MigratePinningPolicy(policies);
}

std::vector<std::string>
DefaultChromeAppsMigrator::RemoveChromeAppsFromExtensionForcelist(
    PolicyMap* policies) const {
  PolicyMap::Entry* forcelist_entry =
      policies->GetMutable(key::kExtensionInstallForcelist);
  if (!forcelist_entry)
    return std::vector<std::string>();

  const base::Value* forcelist_value =
      forcelist_entry->value(base::Value::Type::LIST);
  if (!forcelist_value)
    return std::vector<std::string>();

  std::vector<std::string> chrome_app_ids;
  base::Value::List new_forcelist_value;
  for (const auto& list_entry : forcelist_value->GetList()) {
    if (!list_entry.is_string()) {
      new_forcelist_value.Append(list_entry.Clone());
      continue;
    }

    const std::string entry = list_entry.GetString();
    const size_t pos = entry.find(';');
    const std::string extension_id = entry.substr(0, pos);

    if (chrome_app_to_web_app_.count(extension_id))
      chrome_app_ids.push_back(extension_id);
    else
      new_forcelist_value.Append(entry);
  }

  forcelist_entry->set_value(base::Value(std::move(new_forcelist_value)));
  return chrome_app_ids;
}

void DefaultChromeAppsMigrator::EnsurePolicyValueIsList(
    PolicyMap* policies,
    const std::string& policy_name) const {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* policy_value = policies->GetValueUnsafe(policy_name);
  if (!policy_value || !policy_value->is_list()) {
    const PolicyMap::Entry* forcelist_entry =
        policies->Get(key::kExtensionInstallForcelist);
    PolicyMap::Entry policy_entry(
        forcelist_entry->level, forcelist_entry->scope, forcelist_entry->source,
        base::Value(base::Value::Type::LIST), /*external_data_fetcher=*/nullptr,
        policies->GetPolicyDetails(policy_name));
    // If `policy_value` has wrong type, add message before overriding value.
    if (policy_value) {
      policy_entry.AddMessage(PolicyMap::MessageType::kError,
                              IDS_POLICY_TYPE_ERROR);
    }
    policies->Set(policy_name, std::move(policy_entry));
  }
}

void DefaultChromeAppsMigrator::MigratePinningPolicy(
    PolicyMap* policies) const {
  base::Value* pinned_apps_value = policies->GetMutableValue(
      key::kPinnedLauncherApps, base::Value::Type::LIST);
  if (!pinned_apps_value)
    return;
  for (auto& list_entry : pinned_apps_value->GetList()) {
    if (!list_entry.is_string())
      continue;
    const std::string pinned_app = list_entry.GetString();
    auto it = chrome_app_to_web_app_.find(pinned_app);
    if (it != chrome_app_to_web_app_.end())
      list_entry = base::Value(it->second);
  }
}

}  // namespace policy
