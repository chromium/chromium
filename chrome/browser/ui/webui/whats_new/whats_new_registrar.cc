// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"

#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

namespace whats_new {
using BrowserCommand = browser_command::mojom::Command;
void RegisterWhatsNewModules(whats_new::WhatsNewRegistry* registry) {
  // Register modules here.
  // 129
  registry->RegisterModule(WhatsNewModule(
      "vinnypersky@google.com", BrowserCommand::kOpenPaymentsSettings));
}

void RegisterWhatsNewEditions(whats_new::WhatsNewRegistry* registry) {
  // Register editions here.
}

std::unique_ptr<WhatsNewRegistry> CreateWhatsNewRegistry() {
  auto registry = std::make_unique<WhatsNewRegistry>(
      std::make_unique<WhatsNewStorageServiceImpl>());

  RegisterWhatsNewModules(registry.get());
  // Perform module pref cleanup.
  registry->ClearUnregisteredModules();

  RegisterWhatsNewEditions(registry.get());
  // Perform edition pref cleanup.
  registry->ClearUnregisteredEditions();

  return registry;
}
}  // namespace whats_new
