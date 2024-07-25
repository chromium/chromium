// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_PROVIDER_FACTORY_LACROS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_PROVIDER_FACTORY_LACROS_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace printing {

class ExtensionPrinterServiceProviderLacros;

// Service factory to create ExtensionPrinterServiceProviderLacros per
// BrowserContext. Note that OTR browser context uses its original profile's
// browser context, and won't create a separate
// ExtensionPrinterServiceProviderLacros.
class ExtensionPrinterServiceProviderFactoryLacros
    : public ProfileKeyedServiceFactory {
 public:
  // Returns the ExtensionPrinterServiceProviderLacros for |browser_context|,
  // creating it if it is not yet created.
  static ExtensionPrinterServiceProviderLacros* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns the ExtensionPrinterServiceProviderFactoryLacros instance.
  static ExtensionPrinterServiceProviderFactoryLacros* GetInstance();

 private:
  friend class base::NoDestructor<ExtensionPrinterServiceProviderFactoryLacros>;
  ExtensionPrinterServiceProviderFactoryLacros();
  ~ExtensionPrinterServiceProviderFactoryLacros() override;

  // ProfileKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_PROVIDER_FACTORY_LACROS_H_
