// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_syncable_factory.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_value_store.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/dual_layer_user_pref_store.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace sync_preferences {

PrefServiceSyncableFactory::PrefServiceSyncableFactory() = default;

PrefServiceSyncableFactory::~PrefServiceSyncableFactory() = default;

void PrefServiceSyncableFactory::SetManagedPolicies(
    policy::PolicyService* service,
    policy::BrowserPolicyConnector* connector) {
  set_managed_prefs(new policy::ConfigurationPolicyPrefStore(
      connector, service, connector->GetHandlerList(),
      policy::POLICY_LEVEL_MANDATORY));
}

void PrefServiceSyncableFactory::SetRecommendedPolicies(
    policy::PolicyService* service,
    policy::BrowserPolicyConnector* connector) {
  set_recommended_prefs(new policy::ConfigurationPolicyPrefStore(
      connector, service, connector->GetHandlerList(),
      policy::POLICY_LEVEL_RECOMMENDED));
}

void PrefServiceSyncableFactory::SetPrefModelAssociatorClient(
    scoped_refptr<PrefModelAssociatorClient> pref_model_associator_client) {
  pref_model_associator_client_ = pref_model_associator_client;
}

void PrefServiceSyncableFactory::SetAccountPrefStore(
    scoped_refptr<PersistentPrefStore> account_pref_store) {
  account_pref_store_ = std::move(account_pref_store);
}

std::unique_ptr<PrefServiceSyncable> PrefServiceSyncableFactory::CreateSyncable(
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry) {
  TRACE_EVENT0("browser", "PrefServiceSyncableFactory::CreateSyncable");

  auto pref_notifier = std::make_unique<PrefNotifierImpl>();

  if (base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage)) {
    // If EnablePreferencesAccountStorage is enabled, then a
    // DualLayerUserPrefStore is used as the main user pref store, and sync is
    // hooked up directly to the underlying account store.

    // In some tests, `account_pref_store_` may not have been set.
    // TODO(crbug.com/40283049): Fix all the usages.
    if (!account_pref_store_) {
      CHECK_IS_TEST();
      account_pref_store_ = base::MakeRefCounted<InMemoryPrefStore>();
    }
    auto dual_layer_user_pref_store =
        base::MakeRefCounted<sync_preferences::DualLayerUserPrefStore>(
            user_prefs_, account_pref_store_, pref_model_associator_client_);
    auto pref_value_store = std::make_unique<PrefValueStore>(
        managed_prefs_.get(), supervised_user_prefs_.get(),
        extension_prefs_.get(), standalone_browser_prefs_.get(),
        command_line_prefs_.get(), dual_layer_user_pref_store.get(),
        recommended_prefs_.get(), pref_registry->defaults().get(),
        pref_notifier.get());
    return std::make_unique<PrefServiceSyncable>(
        std::move(pref_notifier), std::move(pref_value_store),
        std::move(dual_layer_user_pref_store), standalone_browser_prefs_,
        std::move(pref_registry), pref_model_associator_client_,
        read_error_callback_, async_);
  }

  auto pref_value_store = std::make_unique<PrefValueStore>(
      managed_prefs_.get(), supervised_user_prefs_.get(),
      extension_prefs_.get(), standalone_browser_prefs_.get(),
      command_line_prefs_.get(), user_prefs_.get(), recommended_prefs_.get(),
      pref_registry->defaults().get(), pref_notifier.get());

  return std::make_unique<PrefServiceSyncable>(
      std::move(pref_notifier), std::move(pref_value_store), user_prefs_,
      standalone_browser_prefs_, std::move(pref_registry),
      pref_model_associator_client_, read_error_callback_, async_);
}

}  // namespace sync_preferences
