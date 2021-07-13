// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

InstallManager::InstallParams::InstallParams() = default;

InstallManager::InstallParams::~InstallParams() = default;

InstallManager::InstallParams::InstallParams(const InstallParams&) = default;

InstallManager::InstallManager(Profile* profile) : profile_(profile) {}

InstallManager::~InstallManager() = default;

void InstallManager::SetSubsystems(WebAppRegistrar* registrar,
                                   OsIntegrationManager* os_integration_manager,
                                   InstallFinalizer* finalizer) {
  registrar_ = registrar;
  os_integration_manager_ = os_integration_manager;
  finalizer_ = finalizer;
}

}  // namespace web_app
