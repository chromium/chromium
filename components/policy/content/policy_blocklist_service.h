// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_

#include <memory>

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
  explicit PolicyBlocklistService(
      std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager);
  PolicyBlocklistService(const PolicyBlocklistService&) = delete;
  PolicyBlocklistService& operator=(const PolicyBlocklistService&) = delete;
  ~PolicyBlocklistService() override;

  policy::URLBlocklist::URLBlocklistState GetURLBlocklistState(
      const GURL& url) const;

 private:
  std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager_;
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
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_
