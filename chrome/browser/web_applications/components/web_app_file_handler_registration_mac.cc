// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

namespace web_app {

namespace {

// UMA metric name for file handler registration result.
constexpr const char* kRegistrationResultMetric =
    "Apps.FileHandler.Registration.Mac.Result";

// Result of file handler registration process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RegistrationResult {
  kSuccess = 0,
  kFailToCreateShortcut = 1,
  kMaxValue = kFailToCreateShortcut
};

void UpdateFileHandlerRegistrationInOs(const AppId& app_id, Profile* profile) {
  // On OSX, file associations are managed through app shims in the Applications
  // directory, so after enabling or disabling file handling for an app its shim
  // needs to be updated.
  OsIntegrationManager& os_integration_manager =
      WebAppProviderBase::GetProviderBase(profile)->os_integration_manager();
  auto onCreateShortcut = [](bool shortcut_created) {
    UMA_HISTOGRAM_ENUMERATION(kRegistrationResultMetric,
                              shortcut_created
                                  ? RegistrationResult::kSuccess
                                  : RegistrationResult::kFailToCreateShortcut);
  };
  os_integration_manager.CreateShortcuts(app_id, /*add_to_desktop=*/false,
                                         base::BindOnce(onCreateShortcut));
}

}  // namespace

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers) {
  UpdateFileHandlerRegistrationInOs(app_id, profile);
}

void UnregisterFileHandlersWithOs(const AppId& app_id, Profile* profile) {
  // If this was triggered as part of the uninstallation process, nothing more
  // is needed. Uninstalling already cleans up app shims (and thus, file
  // handlers).
  auto* provider = WebAppProviderBase::GetProviderBase(profile);
  if (!provider->registrar().IsInstalled(app_id))
    return;

  UpdateFileHandlerRegistrationInOs(app_id, profile);
}

}  // namespace web_app