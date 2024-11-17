// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"
#include "components/lens/lens_features.h"
#include "components/performance_manager/public/features.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "pdf/buildflags.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif

namespace whats_new {
using BrowserCommand = browser_command::mojom::Command;

namespace features {
BASE_FEATURE(kSafetyAwareness,
             "SafetyAwareness",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSpark, "Spark", base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

void RegisterWhatsNewModules(whats_new::WhatsNewRegistry* registry) {
  // Register modules here.
  // 129
  registry->RegisterModule(
      WhatsNewModule("Googlepayreauth", "vinnypersky@google.com",
                     BrowserCommand::kOpenPaymentsSettings));

  // 131
  registry->RegisterModule(WhatsNewModule(
      lens::features::kLensOverlayTranslateButton, "juanmojica@google.com"));

#if BUILDFLAG(ENABLE_PDF)
  // 132
  registry->RegisterModule(WhatsNewModule(chrome_pdf::features::kPdfSearchify,
                                          "rhalavati@chromium.org"));
#endif

  registry->RegisterModule(
      WhatsNewModule(::features::kToolbarPinning, "corising@google.com",
                     BrowserCommand::kShowCustomizeChromeToolbar));

  registry->RegisterModule(
      WhatsNewModule(performance_manager::features::kPerformanceInterventionUI,
                     "agale@google.com"));
}

void RegisterWhatsNewEditions(whats_new::WhatsNewRegistry* registry) {
  // Register editions here.
  // 130
  registry->RegisterEdition(
      WhatsNewEdition(features::kSafetyAwareness, "mickeyburks@google.com"));

  // 131
  registry->RegisterEdition(
      WhatsNewEdition(features::kSpark, "mickeyburks@google.com"));
}

std::unique_ptr<WhatsNewRegistry> CreateWhatsNewRegistry() {
  auto registry = std::make_unique<WhatsNewRegistry>(
      std::make_unique<WhatsNewStorageServiceImpl>());

  RegisterWhatsNewModules(registry.get());
  RegisterWhatsNewEditions(registry.get());

  // In some tests, the pref service may not be initialized. Make sure
  // this has been created before trying to clean up prefs.
  if (g_browser_process->local_state()) {
    // Perform module and edition pref cleanup.
    registry->ClearUnregisteredModules();
    registry->ClearUnregisteredEditions();
  }

  return registry;
}
}  // namespace whats_new
