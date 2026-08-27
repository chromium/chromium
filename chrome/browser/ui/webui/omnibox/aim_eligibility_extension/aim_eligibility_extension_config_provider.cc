// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_config_provider.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_config_map.h"
#include "extensions/browser/extension_config_map_factory.h"
#include "extensions/common/constants.h"

// static
void AimEligibilityExtensionConfigProvider::Register(
    content::BrowserContext* browser_context) {
  extensions::ExtensionConfigMapFactory::GetOrCreateForBrowserContext(
      browser_context)
      ->RegisterConfigProvider(
          std::make_unique<AimEligibilityExtensionConfigProvider>());
}

AimEligibilityExtensionConfigProvider::AimEligibilityExtensionConfigProvider()
    : ExtensionConfigProvider(extension_misc::kAimEligibilityExtensionId) {}

AimEligibilityExtensionConfigProvider::
    ~AimEligibilityExtensionConfigProvider() = default;

base::DictValue AimEligibilityExtensionConfigProvider::GetLoadTimeData(
    content::BrowserContext& context) {
  return OmniboxUI::GetAimEligibilityLoadTimeData(
      Profile::FromBrowserContext(&context));
}

bool AimEligibilityExtensionConfigProvider::IsJsErrorReportingEnabled() const {
  return true;
}

bool AimEligibilityExtensionConfigProvider::
    ShouldCrashOnJsErrorInDevelopmentBuild() const {
  return true;
}
