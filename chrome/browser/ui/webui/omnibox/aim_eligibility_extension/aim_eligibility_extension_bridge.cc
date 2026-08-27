// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_binder_provider.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge_factory.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_config_provider.h"

AimEligibilityExtensionBridge::AimEligibilityExtensionBridge(Profile* profile)
    : service_worker_page_handler_factory_(*profile) {
  AimEligibilityExtensionConfigProvider::Register(profile);
  AimEligibilityExtensionBinderProvider::Register(profile);
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
