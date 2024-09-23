// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

// static
IsolatedWebAppReaderRegistry*
IsolatedWebAppReaderRegistryFactory::GetForProfile(Profile* profile) {
  return static_cast<IsolatedWebAppReaderRegistry*>(
      IsolatedWebAppReaderRegistryFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
IsolatedWebAppReaderRegistryFactory*
IsolatedWebAppReaderRegistryFactory::GetInstance() {
  static base::NoDestructor<IsolatedWebAppReaderRegistryFactory> instance;
  return instance.get();
}

IsolatedWebAppReaderRegistryFactory::IsolatedWebAppReaderRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "IsolatedWebAppReaderRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

IsolatedWebAppReaderRegistryFactory::~IsolatedWebAppReaderRegistryFactory() =
    default;

std::unique_ptr<KeyedService>
IsolatedWebAppReaderRegistryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile& profile = CHECK_DEREF(Profile::FromBrowserContext(context));

  auto validator = std::make_unique<IsolatedWebAppValidator>();
  auto reader_factory = std::make_unique<IsolatedWebAppResponseReaderFactory>(
      profile, std::move(validator));
  return std::make_unique<IsolatedWebAppReaderRegistry>(
      profile, std::move(reader_factory));
}

content::BrowserContext*
IsolatedWebAppReaderRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(context)) {
    return nullptr;
  }

  return GetBrowserContextForWebApps(context);
}

}  // namespace web_app
