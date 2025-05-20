// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_PASSWORD_MANAGER_BLOCKLIST_POLICY_H_
#define COMPONENTS_POLICY_CONTENT_PASSWORD_MANAGER_BLOCKLIST_POLICY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"

// PasswordManagerBlocklistPolicy and
// PasswordManagerBlocklistPolicyFactory provide a way for us to access
// PasswordManagerBlocklistPolicy, a policy block list service based on the
// Preference Service. The PasswordManagerBlocklistPolicy responds to
// permission changes and is per-Profile.
class PasswordManagerBlocklistPolicy : public KeyedService {
 public:
  explicit PasswordManagerBlocklistPolicy(
      std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager);
  PasswordManagerBlocklistPolicy(const PasswordManagerBlocklistPolicy&) =
      delete;
  PasswordManagerBlocklistPolicy& operator=(
      const PasswordManagerBlocklistPolicy&) = delete;
  ~PasswordManagerBlocklistPolicy() override;

  policy::URLBlocklist::URLBlocklistState GetURLBlocklistState(
      const GURL& url) const;

 private:
  std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager_;
};

class PasswordManagerBlocklistPolicyFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  PasswordManagerBlocklistPolicyFactory(
      const PasswordManagerBlocklistPolicyFactory&) = delete;
  PasswordManagerBlocklistPolicyFactory& operator=(
      const PasswordManagerBlocklistPolicyFactory&) = delete;

  static PasswordManagerBlocklistPolicyFactory* GetInstance();
  static PasswordManagerBlocklistPolicy* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  PasswordManagerBlocklistPolicyFactory();
  ~PasswordManagerBlocklistPolicyFactory() override;
  friend struct base::DefaultSingletonTraits<
      PasswordManagerBlocklistPolicyFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // COMPONENTS_POLICY_CONTENT_PASSWORD_MANAGER_BLOCKLIST_POLICY_H_
