// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/web_app_uninstall_dialog.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

WebAppDialogManager::WebAppDialogManager(Profile* profile)
    : profile_(profile) {}

WebAppDialogManager::~WebAppDialogManager() = default;

bool WebAppDialogManager::CanUserUninstallWebApp(const AppId& app_id) const {
  auto* provider = WebAppProvider::GetForWebApps(profile_);
  if (!provider)
    return false;

  return provider->install_finalizer().CanUserUninstallWebApp(app_id);
}

void WebAppDialogManager::UninstallWebApp(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    BrowserWindow* parent_window,
    Callback callback) {
  UninstallWebApp(app_id, uninstall_source,
                  parent_window ? parent_window->GetNativeWindow() : nullptr,
                  std::move(callback));
}

void WebAppDialogManager::UninstallWebApp(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent_window,
    Callback callback) {
  std::unique_ptr<WebAppUninstallDialog> uninstall_dialog =
      WebAppUninstallDialog::Create(profile_,
                                    parent_window ? parent_window : nullptr);

  uninstall_dialog->ConfirmUninstall(
      app_id, uninstall_source,
      base::BindOnce(&WebAppDialogManager::OnWebAppUninstallDialogClosed,
                     base::Unretained(this), uninstall_dialog.get(),
                     std::move(callback)));

  dialogs_.insert(std::move(uninstall_dialog));
}

void WebAppDialogManager::OnWebAppUninstallDialogClosed(
    WebAppUninstallDialog* dialog,
    Callback callback,
    webapps::UninstallResultCode code) {
  DCHECK(dialogs_.contains(dialog));
  dialogs_.erase(dialog);

  std::move(callback).Run(code);
}

}  // namespace web_app
