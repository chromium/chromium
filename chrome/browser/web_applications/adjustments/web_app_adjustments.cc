// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/adjustments/web_app_adjustments.h"

#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace web_app {

// --------------------------------
// WebAppAdjustments implementation
// --------------------------------

WebAppAdjustments::WebAppAdjustments(Profile* profile) {
  if (base::FeatureList::IsEnabled(
          features::kPreinstalledWebAppDuplicationFixer)) {
    preinstalled_web_app_duplication_fixer_ =
        std::make_unique<PreinstalledWebAppDuplicationFixer>(*profile);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(web_app::kWebAppCalculatorAppErasureFixer)) {
    calculator_app_erasure_fixer_ =
        std::make_unique<CalculatorAppErasureFixer>(*profile);
  }
#endif
}

WebAppAdjustments::~WebAppAdjustments() = default;

// ---------------------------------------
// WebAppAdjustmentsFactory implementation
// ---------------------------------------

// static
WebAppAdjustmentsFactory* WebAppAdjustmentsFactory::GetInstance() {
  static base::NoDestructor<WebAppAdjustmentsFactory> instance;
  return instance.get();
}

WebAppAdjustments* WebAppAdjustmentsFactory::Get(Profile* profile) {
  return static_cast<WebAppAdjustments*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

WebAppAdjustmentsFactory::WebAppAdjustmentsFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppAdjustments",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebAppProviderFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

WebAppAdjustmentsFactory::~WebAppAdjustmentsFactory() = default;

KeyedService* WebAppAdjustmentsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new WebAppAdjustments(profile);
}

bool WebAppAdjustmentsFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* WebAppAdjustmentsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return GetBrowserContextForWebApps(context) &&
                 apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
                     profile)
             ? context
             : nullptr;
}

void WebAppAdjustmentsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CalculatorAppErasureFixer::RegisterProfilePrefs(registry);
#endif
}

}  // namespace web_app
