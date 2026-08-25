// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_binder_provider.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge_factory.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_config_provider.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"

AimEligibilityExtensionBridge::AimEligibilityExtensionBridge(Profile* profile)
    : profile_(*profile), service_worker_page_handler_factory_(*profile) {
  AimEligibilityExtensionConfigProvider::Register(profile);
  AimEligibilityExtensionBinderProvider::Register(profile);

  auto* resource_manager = extensions::ExtensionsBrowserClient::Get()
                               ->GetComponentExtensionResourceManager();
  if (resource_manager) {
    load_time_data_subscription_ =
        resource_manager->RegisterTemplateDataProvider(
            extension_misc::kAimEligibilityExtensionId, &profile_.get(),
            base::BindRepeating(&AimEligibilityExtensionBridge::GetLoadTimeData,
                                base::Unretained(this)));
  }
}

AimEligibilityExtensionBridge::~AimEligibilityExtensionBridge() = default;

// static
AimEligibilityExtensionBridge* AimEligibilityExtensionBridge::Get(
    Profile* profile) {
  return AimEligibilityExtensionBridgeFactory::GetForProfile(profile);
}

void AimEligibilityExtensionBridge::Shutdown() {
  service_worker_page_handler_factory_.Clear();
}

base::DictValue AimEligibilityExtensionBridge::GetLoadTimeData() {
  base::DictValue dict;
  // The following keys (`aimEligibilityTitle`, `showAimEligibilityFooter`) are
  // for illustration and parity purposes with the WebUI version.
  dict.Set("aimEligibilityTitle", "AIM Eligibility Diagnostic");
  dict.Set("showAimEligibilityFooter", true);
  return dict;
}
