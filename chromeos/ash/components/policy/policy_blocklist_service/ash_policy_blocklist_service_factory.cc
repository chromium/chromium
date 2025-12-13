// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/policy_blocklist_service/ash_policy_blocklist_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
AshPolicyBlocklistServiceFactory*
AshPolicyBlocklistServiceFactory::GetInstance() {
  static base::NoDestructor<AshPolicyBlocklistServiceFactory> instance;
  return instance.get();
}

// static
PolicyBlocklistService* AshPolicyBlocklistServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PolicyBlocklistService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AshPolicyBlocklistServiceFactory::AshPolicyBlocklistServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AshPolicyBlocklistService",
          BrowserContextDependencyManager::GetInstance()) {}

AshPolicyBlocklistServiceFactory::~AshPolicyBlocklistServiceFactory() = default;

std::unique_ptr<KeyedService>
AshPolicyBlocklistServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  PrefService* pref_service = user_prefs::UserPrefs::Get(context);
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      pref_service, policy::policy_prefs::kUrlBlocklist,
      policy::policy_prefs::kUrlAllowlist);
  return std::make_unique<PolicyBlocklistService>(
      std::move(url_blocklist_manager), pref_service);
}

content::BrowserContext*
AshPolicyBlocklistServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace ash
