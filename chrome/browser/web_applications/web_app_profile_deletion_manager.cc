// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_profile_deletion_manager.h"

#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

WebAppProfileDeletionManager::WebAppProfileDeletionManager(Profile* profile)
    : profile_(profile) {}

WebAppProfileDeletionManager::~WebAppProfileDeletionManager() = default;

void WebAppProfileDeletionManager::SetProvider(base::PassKey<WebAppProvider>,
                                               WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppProfileDeletionManager::Start() {
  // `ProfileManager` can be null in unit-tests.
  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(profile_manager);
  }
}

void WebAppProfileDeletionManager::Shutdown() {
  profile_manager_observation_.Reset();
}

void WebAppProfileDeletionManager::OnProfileMarkedForPermanentDeletion(
    Profile* profile_to_be_deleted) {
  if (profile_ != profile_to_be_deleted) {
    return;
  }
  RemoveDataForProfileDeletion();
}

void WebAppProfileDeletionManager::OnProfileManagerDestroying() {
  // Shut down the command system, aborting all running commands synchronously.
  // This helps destroy the `WebContents` instance that might be created by the
  // `command_manager()` before profile destruction has started.
  // This serves as a crash fix for crbug.com/415776884.
  provider_->command_manager().Shutdown();
  profile_manager_observation_.Reset();
}

void WebAppProfileDeletionManager::RemoveDataForProfileDeletion() {
  // First shut down the command system, aborting all running commands
  // synchronously.
  provider_->command_manager().Shutdown();

  // Second, remove all OS integration for the profile.
  WebAppRegistrar& registrar = provider_->registrar_unsafe();

  for (const webapps::AppId& app_id : registrar.GetAppIds()) {
    provider_->os_integration_manager()
        .UnregisterOsIntegrationOnProfileMarkedForDeletion(
            base::PassKey<WebAppProfileDeletionManager>(), app_id);
  }
}

}  // namespace web_app
