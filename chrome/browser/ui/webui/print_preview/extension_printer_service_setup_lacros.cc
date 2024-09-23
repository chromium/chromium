// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_service_setup_lacros.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_lacros.h"

namespace printing {

ExtensionPrinterServiceSetupLacros*
ExtensionPrinterServiceSetupLacros::GetInstance() {
  static base::NoDestructor<ExtensionPrinterServiceSetupLacros>
      extension_printer_service_setup_lacros;
  return extension_printer_service_setup_lacros.get();
}

ExtensionPrinterServiceSetupLacros::ExtensionPrinterServiceSetupLacros() {
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
}

ExtensionPrinterServiceSetupLacros::~ExtensionPrinterServiceSetupLacros() =
    default;

void ExtensionPrinterServiceSetupLacros::OnProfileAdded(Profile* profile) {
  // Create ExtensionPrinterServiceProviderLacros for |profile| if it is a main
  // profile.
  if (profile == ProfileManager::GetPrimaryUserProfile()) {
    ExtensionPrinterServiceProviderLacros::GetForBrowserContext(profile);
  }
}

void ExtensionPrinterServiceSetupLacros::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

}  // namespace printing
