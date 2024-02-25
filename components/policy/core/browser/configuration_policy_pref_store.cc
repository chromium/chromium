// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_pref_store.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

namespace {

void LogErrors(std::unique_ptr<PolicyErrorMap> errors,
               PoliciesSet deprecated_policies,
               PoliciesSet future_policies) {
  DCHECK(errors->IsReady());
  for (auto& pair : *errors) {
    const auto& policy = pair.first;
    DLOG_POLICY(WARNING, POLICY_PROCESSING)
        << "Policy " << policy << ": " << pair.second.message;
  }
  for (const auto& policy : deprecated_policies) {
    VLOG_POLICY(1, POLICY_PROCESSING)
        << "Policy " << policy << " has been deprecated.";
  }
  for (const auto& policy : future_policies) {
    VLOG_POLICY(1, POLICY_PROCESSING)
        << "Policy " << policy << " has not been released yet.";
  }
}

bool IsLevel(PolicyLevel level, PolicyMap::const_reference iter) {
  return iter.second.level == level;
}

}  // namespace

ConfigurationPolicyPrefStore::ConfigurationPolicyPrefStore(
    BrowserPolicyConnectorBase* policy_connector,
    PolicyService* service,
    const ConfigurationPolicyHandlerList* handler_list,
    PolicyLevel level)
    : policy_connector_(policy_connector),
      policy_service_(service),
      handler_list_(handler_list),
      level_(level),
      prefs_(CreatePreferencesFromPolicies()) {
  // `prefs_` starts out with the initial policy.
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
}

void ConfigurationPolicyPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void ConfigurationPolicyPrefStore::RemoveObserver(
    PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ConfigurationPolicyPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool ConfigurationPolicyPrefStore::IsInitializationComplete() const {
  return policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME);
}

bool ConfigurationPolicyPrefStore::GetValue(std::string_view key,
                                            const base::Value** value) const {
  const base::Value* stored_value = nullptr;
  if (!prefs_ || !prefs_->GetValue(key, &stored_value))
    return false;

  if (value)
    *value = stored_value;
  return true;
}

base::Value::Dict ConfigurationPolicyPrefStore::GetValues() const {
  if (!prefs_)
    return base::Value::Dict();
  return prefs_->AsDict();
}

void ConfigurationPolicyPrefStore::OnPolicyUpdated(const PolicyNamespace& ns,
                                                   const PolicyMap& previous,
                                                   const PolicyMap& current) {
  DCHECK_EQ(POLICY_DOMAIN_CHROME, ns.domain);
  DCHECK(ns.component_id.empty());
  Refresh();
}

void ConfigurationPolicyPrefStore::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain == POLICY_DOMAIN_CHROME) {
    for (auto& observer : observers_)
      observer.OnInitializationCompleted(true);
  }
}

ConfigurationPolicyPrefStore::~ConfigurationPolicyPrefStore() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void ConfigurationPolicyPrefStore::Refresh() {
  std::unique_ptr<PrefValueMap> new_prefs = CreatePreferencesFromPolicies();
  std::vector<std::string> changed_prefs;
  new_prefs->GetDifferingKeys(prefs_.get(), &changed_prefs);
  prefs_.swap(new_prefs);

  // Send out change notifications.
  for (const auto& pref : changed_prefs) {
    for (auto& observer : observers_)
      observer.OnPrefValueChanged(pref);
  }
}

std::unique_ptr<PrefValueMap>
ConfigurationPolicyPrefStore::CreatePreferencesFromPolicies() {
  auto prefs = std::make_unique<PrefValueMap>();
  PolicyMap filtered_policies =
      policy_service_
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .CloneIf(base::BindRepeating(&IsLevel, level_));

  auto errors = std::make_unique<PolicyErrorMap>();

  PoliciesSet deprecated_policies;
  PoliciesSet future_policies;
  handler_list_->ApplyPolicySettings(filtered_policies, prefs.get(),
                                     errors.get(), &deprecated_policies,
                                     &future_policies);

  if (!errors->empty()) {
    if (errors->IsReady()) {
      LogErrors(std::move(errors), std::move(deprecated_policies),
                std::move(future_policies));
    } else if (policy_connector_) {  // May be null in tests.
      policy_connector_->NotifyWhenResourceBundleReady(base::BindOnce(
          &LogErrors, std::move(errors), std::move(deprecated_policies),
          std::move(future_policies)));
    }
  }

  return prefs;
}

}  // namespace policy
