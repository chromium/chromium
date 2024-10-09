// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

// Use CHECK instead of DCHECK if a constraint is coming from client side. We
// release this feature via channel based release. Those CHECKs should be caught
// during the process. Note that DCHECK and a fail-safe behavior should be
// used/implemented if a constraint is coming from server side or a config.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH) ScalableIphFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  using OnBuildingServiceInstanceForTestingCallback =
      base::RepeatingCallback<void(content::BrowserContext* browser_context)>;

  static ScalableIphFactory* GetInstance();

  static scalable_iph::ScalableIph* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Call `GetBrowserContextToUse` with a logger for debugging purpose.
  // `GetBrowserContextToUse` is a const member function. We have to pass a
  // logger from the outside. This function is also marked as const to avoid
  // accidentally changing its internal state.
  virtual content::BrowserContext* GetBrowserContextToUseForDebug(
      content::BrowserContext* browser_context,
      scalable_iph::Logger* logger) const = 0;

  // `ScalableIph` service has a repeating timer in it to invoke time tick
  // events. We want to start this service after a user login (but not during
  // OOBE session). A service must be created via this method to make sure it
  // happen. `GetForBrowserContext` does NOT instantiate a service.
  void InitializeServiceForBrowserContext(
      content::BrowserContext* browser_context);

  // Allows test code to set up test environment before a service is built. This
  // is called just before a service is built. For now, this is used to set
  // can_use_manta_service capability. ScalableIph service is built immediately
  // after user session starts. It's non-trival to do env set up at the correct
  // timing without this type of helper method.
  void SetOnBuildingServiceInstanceForTestingCallback(
      OnBuildingServiceInstanceForTestingCallback callback);

 protected:
  ScalableIphFactory();
  ~ScalableIphFactory() override;

 private:
  OnBuildingServiceInstanceForTestingCallback
      on_building_service_instance_for_testing_callback_;
};

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_
