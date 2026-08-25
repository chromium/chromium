// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_CONFIG_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_CONFIG_PROVIDER_H_

#include "extensions/browser/extension_config_map.h"

namespace content {
class BrowserContext;
}

// Provides configuration for the Aim Eligibility component extension.
class AimEligibilityExtensionConfigProvider
    : public extensions::ExtensionConfigProvider {
 public:
  static void Register(content::BrowserContext* browser_context);

  AimEligibilityExtensionConfigProvider();
  ~AimEligibilityExtensionConfigProvider() override;

  bool IsJsErrorReportingEnabled() const override;
  bool ShouldCrashOnJsErrorInDevelopmentBuild() const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_CONFIG_PROVIDER_H_
