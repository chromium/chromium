// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace web_app {

namespace {

void RegisterRunOnOsLoginAndPostCallback(ResultCallback callback,
                                         const ShortcutInfo& shortcut_info) {
  bool run_on_os_login_registered =
      internals::RegisterRunOnOsLogin(shortcut_info);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), run_on_os_login_registered
                                                         ? Result::kOk
                                                         : Result::kError));
}

}  // namespace

void ScheduleRegisterRunOnOsLogin(WebAppSyncBridge* sync_bridge,
                                  std::unique_ptr<ShortcutInfo> shortcut_info,
                                  ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(sync_bridge);

  // TODO(crbug.com/1401125): Remove once sub managers have been implemented and
  //  OsIntegrationManager::Synchronize() is running fine.
  if (!AreSubManagersExecuteEnabled()) {
    ScopedRegistryUpdate update(sync_bridge);
    update->UpdateApp(shortcut_info->app_id)
        ->SetRunOnOsLoginOsIntegrationState(RunOnOsLoginMode::kWindowed);
  }

  internals::PostShortcutIOTask(
      base::BindOnce(&RegisterRunOnOsLoginAndPostCallback, std::move(callback)),
      std::move(shortcut_info));
}

void ScheduleUnregisterRunOnOsLogin(WebAppSyncBridge* sync_bridge,
                                    const std::string& app_id,
                                    const base::FilePath& profile_path,
                                    const std::u16string& shortcut_title,
                                    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(sync_bridge);

  // TODO(crbug.com/1401125): Remove once sub managers have been implemented and
  //  OsIntegrationManager::Synchronize() is running fine.
  if (!AreSubManagersExecuteEnabled() &&
      sync_bridge->registrar().IsInstalled(app_id)) {
    ScopedRegistryUpdate update(sync_bridge);
    update->UpdateApp(app_id)->SetRunOnOsLoginOsIntegrationState(
        RunOnOsLoginMode::kNotRun);
  }

  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&internals::UnregisterRunOnOsLogin, app_id, profile_path,
                     shortcut_title),
      std::move(callback));
}

}  // namespace web_app
