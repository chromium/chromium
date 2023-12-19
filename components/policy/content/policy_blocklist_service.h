// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"

// PolicyBlocklistService and PolicyBlocklistFactory provide a way for
// us to access URLBlocklistManager, a policy block list service based on
// the Preference Service. The URLBlocklistManager responds to permission
// changes and is per-Profile.
class PolicyBlocklistService : public KeyedService {
 public:
  PolicyBlocklistService(
      std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager,
      PrefService* user_prefs);

  PolicyBlocklistService(const PolicyBlocklistService&) = delete;
  PolicyBlocklistService& operator=(const PolicyBlocklistService&) = delete;
  ~PolicyBlocklistService() override;

  policy::URLBlocklist::URLBlocklistState GetURLBlocklistState(
      const GURL& url) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Configures the URL filters source the `url_blocklist_manager_`. If
  // `enforced` is false, the default URL filters source is used (i.e. the
  // URLBlocklist and URLAllowlist prefs). If `enforced` is true, the
  // `url_blocklist_manager_` is configured to use a custom source for URL
  // filters.
  void SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(bool enforced);
#endif

 private:
  std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager_;
  raw_ptr<PrefService> user_prefs_;
};

class PolicyBlocklistFactory : public BrowserContextKeyedServiceFactory {
 public:
  PolicyBlocklistFactory(const PolicyBlocklistFactory&) = delete;
  PolicyBlocklistFactory& operator=(const PolicyBlocklistFactory&) = delete;

  static PolicyBlocklistFactory* GetInstance();
  static PolicyBlocklistService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  PolicyBlocklistFactory();
  ~PolicyBlocklistFactory() override;
  friend struct base::DefaultSingletonTraits<PolicyBlocklistFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_
