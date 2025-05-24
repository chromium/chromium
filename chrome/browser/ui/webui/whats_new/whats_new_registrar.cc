// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/lens/lens_features.h"
#include "components/performance_manager/public/features.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "pdf/buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif

namespace whats_new {
using BrowserCommand = browser_command::mojom::Command;

namespace features {
// Define local features here.
}  // namespace features

void RegisterWhatsNewModules(whats_new::WhatsNewRegistry* registry) {
  // Register modules here.
  // 129
  registry->RegisterModule(
      WhatsNewModule("Googlepayreauth", "vinnypersky@google.com",
                     BrowserCommand::kOpenPaymentsSettings));

#if BUILDFLAG(ENABLE_PDF)
  // 132
  registry->RegisterModule(WhatsNewModule(chrome_pdf::features::kPdfSearchify,
                                          "rhalavati@chromium.org"));
#endif

  registry->RegisterModule(
      WhatsNewModule(::features::kReadAnythingReadAloud, "trewin@google.com"));

  // M138
  registry->RegisterModule(
      WhatsNewModule("TabGroupsSync", "dpenning@google.com"));
}

void RegisterWhatsNewEditions(whats_new::WhatsNewRegistry* registry) {
  // Register editions here.
#if BUILDFLAG(ENABLE_GLIC)
  registry->RegisterEdition(WhatsNewEdition(
      ::features::kGlicRollout, "tommasin@chromium.org",
      std::vector<BrowserCommand>{BrowserCommand::kOpenGlic,
                                  BrowserCommand::kOpenGlicSettings}));
#endif  // BUILDFLAG(ENABLE_GLIC)
}

std::unique_ptr<WhatsNewRegistry> CreateWhatsNewRegistry() {
  auto registry = std::make_unique<WhatsNewRegistry>(
      std::make_unique<WhatsNewStorageServiceImpl>());

  RegisterWhatsNewEditions(registry.get());

  // In some tests, the pref service may not be initialized. Make sure
  // this has been created before performing operations that rely on local
  // state.
  if (g_browser_process->local_state()) {
    RegisterWhatsNewModules(registry.get());

    // Perform module and edition pref cleanup.
    registry->ClearUnregisteredModules();
    registry->ClearUnregisteredEditions();
  }

  return registry;
}
}  // namespace whats_new
