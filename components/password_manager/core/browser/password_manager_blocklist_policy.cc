// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_blocklist_policy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

PasswordManagerBlocklistPolicy::PasswordManagerBlocklistPolicy(
    std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager)
    : url_blocklist_manager_(std::move(url_blocklist_manager)) {}

PasswordManagerBlocklistPolicy::~PasswordManagerBlocklistPolicy() = default;

policy::URLBlocklist::URLBlocklistState
PasswordManagerBlocklistPolicy::GetURLBlocklistState(const GURL& url) const {
  return url_blocklist_manager_->GetURLBlocklistState(url);
}

// static
PasswordManagerBlocklistPolicyFactory*
PasswordManagerBlocklistPolicyFactory::GetInstance() {
  return base::Singleton<PasswordManagerBlocklistPolicyFactory>::get();
}

// static
PasswordManagerBlocklistPolicy*
PasswordManagerBlocklistPolicyFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PasswordManagerBlocklistPolicy*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PasswordManagerBlocklistPolicyFactory::PasswordManagerBlocklistPolicyFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordManagerBlocklist",
          BrowserContextDependencyManager::GetInstance()) {}

PasswordManagerBlocklistPolicyFactory::
    ~PasswordManagerBlocklistPolicyFactory() = default;

std::unique_ptr<KeyedService>
PasswordManagerBlocklistPolicyFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  PrefService* pref_service = user_prefs::UserPrefs::Get(context);
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      pref_service,
      std::string(password_manager::prefs::kPasswordManagerBlocklist),
      std::nullopt);
  return base::WrapUnique<KeyedService>(
      new PasswordManagerBlocklistPolicy(std::move(url_blocklist_manager)));
}

content::BrowserContext*
PasswordManagerBlocklistPolicyFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
