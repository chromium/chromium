// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_factory_lacros.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_lacros.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace printing {

ExtensionPrinterServiceProviderLacros*
ExtensionPrinterServiceProviderFactoryLacros::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionPrinterServiceProviderLacros*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ExtensionPrinterServiceProviderFactoryLacros*
ExtensionPrinterServiceProviderFactoryLacros::GetInstance() {
  static base::NoDestructor<ExtensionPrinterServiceProviderFactoryLacros>
      instance;
  return instance.get();
}

ExtensionPrinterServiceProviderFactoryLacros::
    ExtensionPrinterServiceProviderFactoryLacros()
    : ProfileKeyedServiceFactory(
          "ExtensionPrinterServiceProviderLacros",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

ExtensionPrinterServiceProviderFactoryLacros::
    ~ExtensionPrinterServiceProviderFactoryLacros() = default;

std::unique_ptr<KeyedService> ExtensionPrinterServiceProviderFactoryLacros::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<ExtensionPrinterServiceProviderLacros>(context);
}

}  // namespace printing
