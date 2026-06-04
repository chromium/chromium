// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/lens/lens_features.h"
#include "components/performance_manager/public/features.h"
#include "components/search/ntp_features.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

namespace whats_new {
using BrowserCommand = browser_command::mojom::Command;

namespace features {
// Define local features here.
}  // namespace features

void RegisterWhatsNewModules(whats_new::WhatsNewRegistry* registry) {
  // Register modules here.

  // M147
  registry->RegisterModule(WhatsNewModule(tabs::kVerticalTabsLaunch,
                                          "charlesmeng@google.com",
                                          BrowserCommand::kEnableVerticalTabs));
}

void RegisterWhatsNewEditions(whats_new::WhatsNewRegistry* registry) {
  // Register editions here.
  registry->RegisterEdition(WhatsNewEdition(
      ::features::kGlicRollout, "tommasin@chromium.org",
      std::vector<BrowserCommand>{BrowserCommand::kOpenGlic,
                                  BrowserCommand::kOpenGlicSettings,
                                  BrowserCommand::kPrewarmGlicFre}));
  registry->RegisterEdition(
      WhatsNewEdition(ntp_features::kLightningTakeoverEdition,
                      "rtatum@google.com", std::vector<BrowserCommand>{}));
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
