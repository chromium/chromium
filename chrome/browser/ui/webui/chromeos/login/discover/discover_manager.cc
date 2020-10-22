// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_manager.h"

#include <algorithm>

#include "base/check_op.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_pin_setup.h"

namespace chromeos {
namespace {

// Owned by ChromeBrowserMainPartsChromeos.
DiscoverManager* g_discover_manager = nullptr;

}  // namespace

DiscoverManager::DiscoverManager() {
  DCHECK(!g_discover_manager);
  g_discover_manager = this;

  CreateModules();
}

DiscoverManager::~DiscoverManager() {
  DCHECK_EQ(g_discover_manager, this);
  g_discover_manager = nullptr;
}

// static
DiscoverManager* DiscoverManager::Get() {
  return g_discover_manager;
}

bool DiscoverManager::IsCompleted() const {
  // Returns true if all of the modules are completed.
  return std::all_of(modules_.begin(), modules_.end(),
                     [](const auto& module_pair) {
                       return module_pair.second->IsCompleted();
                     });
}

void DiscoverManager::CreateModules() {
  modules_[DiscoverModulePinSetup::kModuleName] =
      std::make_unique<DiscoverModulePinSetup>();
}

std::vector<std::unique_ptr<DiscoverHandler>>
DiscoverManager::CreateWebUIHandlers(
    JSCallsContainer* js_calls_container) const {
  std::vector<std::unique_ptr<DiscoverHandler>> handlers;
  for (const auto& module_pair : modules_) {
    handlers.emplace_back(
        module_pair.second->CreateWebUIHandler(js_calls_container));
  }
  return handlers;
}

DiscoverModule* DiscoverManager::GetModuleByName(
    const std::string& module_name) const {
  const auto it = modules_.find(module_name);
  return it == modules_.end() ? nullptr : it->second.get();
}

}  // namespace chromeos
