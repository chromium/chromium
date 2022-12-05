// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1231621): Implement some/all of these once Fuchsia supports
// them.

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/task/task_runner.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_constants.h"

#include "base/notreached.h"

namespace web_app {

bool ShouldRegisterFileHandlersWithOs() {
  return false;
}

bool FileHandlingIconsSupportedByOs() {
  return false;
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers,
                                ResultCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(Result::kError);
}

void UnregisterFileHandlersWithOs(const AppId& app_id,
                                  Profile* profile,
                                  ResultCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(Result::kError);
}

namespace internals {

bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info) {
  NOTIMPLEMENTED();
  return false;
}

bool UnregisterRunOnOsLogin(const std::string& app_id,
                            const base::FilePath& profile_path,
                            const std::u16string& shortcut_title) {
  NOTIMPLEMENTED();
  return false;
}

bool CreatePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info) {
  NOTIMPLEMENTED();
  return false;
}

Result UpdatePlatformShortcuts(const base::FilePath& web_app_path,
                               const std::u16string& old_app_title,
                               const ShortcutInfo& shortcut_info) {
  NOTIMPLEMENTED();
  return Result::kOk;
}

void DeletePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutInfo& shortcut_info,
                             scoped_refptr<base::TaskRunner> result_runner,
                             DeleteShortcutsCallback callback) {
  result_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback), false));
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
  NOTIMPLEMENTED();
}

ShortcutLocations GetAppExistingShortCutLocationImpl(
    const ShortcutInfo& shortcut_info) {
  NOTIMPLEMENTED();
  return {};
}

}  // namespace internals

}  // namespace web_app
