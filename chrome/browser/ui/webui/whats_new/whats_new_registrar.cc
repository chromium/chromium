// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"

namespace whats_new {
void RegisterWhatsNewModules(whats_new::WhatsNewRegistry* registry) {
  // Register modules here.
}

void RegisterWhatsNewEditions(whats_new::WhatsNewRegistry* registry) {
  // Register editions here.
}

std::unique_ptr<WhatsNewRegistry> CreateWhatsNewRegistry() {
  auto registry = std::make_unique<WhatsNewRegistry>();

  RegisterWhatsNewModules(registry.get());
  RegisterWhatsNewEditions(registry.get());

  return registry;
}
}  // namespace whats_new
