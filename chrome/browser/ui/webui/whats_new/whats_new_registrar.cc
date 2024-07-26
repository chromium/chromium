// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"

namespace whats_new {
void RegisterWhatsNewModules(whats_new::WhatsNewRegistry* registry) {
  // Register modules here.
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
