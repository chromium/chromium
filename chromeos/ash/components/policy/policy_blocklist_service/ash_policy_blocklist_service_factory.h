// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_POLICY_BLOCKLIST_SERVICE_ASH_POLICY_BLOCKLIST_SERVICE_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_POLICY_BLOCKLIST_SERVICE_ASH_POLICY_BLOCKLIST_SERVICE_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class PolicyBlocklistService;

namespace ash {

// Factory for PolicyBlocklistService in ash.
// The reason Ash has a special factory is that chrome/browser/ash/ shouldn't
// receive new //chrome dependencies (crbug.com/332804822).
// ChromePolicyBlocklistServiceFactory defers to this factory for non-incognito
// profiles on ChromeOS. If the ash requirements ever change, this should be
// merged with ChromePolicyBlocklistServiceFactory, as this setup is rather
// confusing.
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/DEPS;l=11;drc=ecce9a63db1b3bc7b616f0c9222d93233c587fd6
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    AshPolicyBlocklistServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AshPolicyBlocklistServiceFactory* GetInstance();

  static PolicyBlocklistService* GetForBrowserContext(
      content::BrowserContext* context);

  AshPolicyBlocklistServiceFactory(const AshPolicyBlocklistServiceFactory&) =
      delete;
  AshPolicyBlocklistServiceFactory& operator=(
      const AshPolicyBlocklistServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AshPolicyBlocklistServiceFactory>;

  AshPolicyBlocklistServiceFactory();
  ~AshPolicyBlocklistServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_POLICY_BLOCKLIST_SERVICE_ASH_POLICY_BLOCKLIST_SERVICE_FACTORY_H_
