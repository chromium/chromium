// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

PolicyBlocklistService::PolicyBlocklistService(
    std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager)
    : url_blocklist_manager_(std::move(url_blocklist_manager)) {}

PolicyBlocklistService::~PolicyBlocklistService() = default;

policy::URLBlocklist::URLBlocklistState
PolicyBlocklistService::GetURLBlocklistState(const GURL& url) const {
  return url_blocklist_manager_->GetURLBlocklistState(url);
}

// static
PolicyBlocklistFactory* PolicyBlocklistFactory::GetInstance() {
  return base::Singleton<PolicyBlocklistFactory>::get();
}

// static
PolicyBlocklistService* PolicyBlocklistFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PolicyBlocklistService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PolicyBlocklistFactory::PolicyBlocklistFactory()
    : BrowserContextKeyedServiceFactory(
          "PolicyBlocklist",
          BrowserContextDependencyManager::GetInstance()) {}

PolicyBlocklistFactory::~PolicyBlocklistFactory() = default;

KeyedService* PolicyBlocklistFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  PrefService* pref_service = user_prefs::UserPrefs::Get(context);
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      pref_service, policy::policy_prefs::kUrlBlocklist,
      policy::policy_prefs::kUrlAllowlist);
  return new PolicyBlocklistService(std::move(url_blocklist_manager));
}

content::BrowserContext* PolicyBlocklistFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
