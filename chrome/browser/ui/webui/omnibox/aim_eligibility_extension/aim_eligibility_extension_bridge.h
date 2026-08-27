// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_H_

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_service_worker_page_handler_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// Profile-keyed service that manages the Aim Eligibility component extension's
// integration with the browser. It owns Profile-scoped extension resources,
// such as the Mojo service worker page handler factory, and initializes
// extension configuration and Mojo binder providers.
class AimEligibilityExtensionBridge : public KeyedService {
 public:
  explicit AimEligibilityExtensionBridge(Profile* profile);

  AimEligibilityExtensionBridge(const AimEligibilityExtensionBridge&) = delete;
  AimEligibilityExtensionBridge& operator=(
      const AimEligibilityExtensionBridge&) = delete;

  ~AimEligibilityExtensionBridge() override;

  // KeyedService:
  void Shutdown() override;

  static AimEligibilityExtensionBridge* Get(Profile* profile);

  AimEligibilityExtensionServiceWorkerPageHandlerFactory&
  service_worker_page_handler_factory() {
    return service_worker_page_handler_factory_;
  }

 private:
  AimEligibilityExtensionServiceWorkerPageHandlerFactory
      service_worker_page_handler_factory_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_H_
