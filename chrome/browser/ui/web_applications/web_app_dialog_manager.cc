// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"

#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"

namespace web_app {

namespace {

const extensions::Extension* GetExtension(Profile* profile,
                                          const AppId& app_id) {
  return extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING);
}

}  // namespace

// TODO(loyso): Make it extensions independent.
class DialogInstance : public extensions::ExtensionUninstallDialog::Delegate {
 public:
  DialogInstance() = default;
  ~DialogInstance() override = default;

  void UninstallWebApp(Profile* profile,
                       BrowserWindow* parent_window,
                       const AppId& app_id,
                       WebAppDialogManager::UninstallSource source,
                       WebAppDialogManager::Callback callback) {
    uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
        profile, parent_window ? parent_window->GetNativeWindow() : nullptr,
        this);

    callback_ = std::move(callback);

    auto* app = GetExtension(profile, app_id);
    uninstall_dialog_->ConfirmUninstall(
        app, extensions::UNINSTALL_REASON_USER_INITIATED,
        ConvertSource(source));
  }

 private:
  static extensions::UninstallSource ConvertSource(
      WebAppDialogManager::UninstallSource source) {
    switch (source) {
      case WebAppDialogManager::UninstallSource::kAppMenu:
        return extensions::UNINSTALL_SOURCE_HOSTED_APP_MENU;
      case WebAppDialogManager::UninstallSource::kAppsPage:
        return extensions::UNINSTALL_SOURCE_CHROME_APPS_PAGE;
    }
  }

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool success,
                                        const base::string16& error) override {
    // The dialog can be closed by UI system whenever it likes, but
    // OnExtensionUninstallDialogClosed will be called anyway.
    if (callback_)
      std::move(callback_).Run(success);
  }

  std::unique_ptr<extensions::ExtensionUninstallDialog> uninstall_dialog_;
  WebAppDialogManager::Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(DialogInstance);
};

WebAppDialogManager::WebAppDialogManager(Profile* profile)
    : profile_(profile) {}

WebAppDialogManager::~WebAppDialogManager() = default;

bool WebAppDialogManager::CanUninstallWebApp(const AppId& app_id) const {
  auto* provider = WebAppProvider::Get(profile_);
  if (!provider)
    return false;

  return provider->install_finalizer().CanUserUninstallFromSync(app_id);
}

void WebAppDialogManager::UninstallWebApp(const AppId& app_id,
                                          UninstallSource uninstall_source,
                                          BrowserWindow* parent_window,
                                          Callback callback) {
  auto dialog = std::make_unique<DialogInstance>();

  dialog->UninstallWebApp(
      profile_, parent_window, app_id, uninstall_source,
      base::BindOnce(&WebAppDialogManager::OnDialogCompleted,
                     base::Unretained(this), dialog.get(),
                     std::move(callback)));

  dialogs_.insert(std::move(dialog));
}

void WebAppDialogManager::OnDialogCompleted(DialogInstance* dialog,
                                            Callback callback,
                                            bool success) {
  DCHECK(dialogs_.contains(dialog));
  dialogs_.erase(dialog);

  std::move(callback).Run(success);
}

}  // namespace web_app
