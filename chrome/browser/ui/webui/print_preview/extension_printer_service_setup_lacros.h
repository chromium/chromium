// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_SETUP_LACROS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_SETUP_LACROS_H_

#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

namespace printing {

// Creates ExtensionPrinterServiceProviderLacros when user profile is loaded.
class ExtensionPrinterServiceSetupLacros : public ProfileManagerObserver {
 public:
  static ExtensionPrinterServiceSetupLacros* GetInstance();

  ExtensionPrinterServiceSetupLacros(
      const ExtensionPrinterServiceSetupLacros&) = delete;
  ExtensionPrinterServiceSetupLacros& operator=(
      const ExtensionPrinterServiceSetupLacros&) = delete;

 private:
  friend class base::NoDestructor<ExtensionPrinterServiceSetupLacros>;
  ExtensionPrinterServiceSetupLacros();
  ~ExtensionPrinterServiceSetupLacros() override;

  // ProfileManagerObserver
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_SETUP_LACROS_H_
