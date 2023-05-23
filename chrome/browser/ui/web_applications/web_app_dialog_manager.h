// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_MANAGER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "ui/gfx/native_widget_types.h"

class BrowserWindow;
class Profile;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}

namespace web_app {

class WebAppUninstallDialog;

class WebAppDialogManager {
 public:
  explicit WebAppDialogManager(Profile* profile);
  WebAppDialogManager(const WebAppDialogManager&) = delete;
  WebAppDialogManager& operator=(const WebAppDialogManager&) = delete;
  ~WebAppDialogManager();

  using Callback = base::OnceCallback<void(webapps::UninstallResultCode code)>;

  // The uninstall dialog will be modal to |parent_window|, or a non-modal if
  // |parent_window| is nullptr.
  void UninstallWebApp(const AppId& app_id,
                       webapps::WebappUninstallSource uninstall_source,
                       BrowserWindow* parent_window,
                       Callback callback);

  void UninstallWebApp(const AppId& app_id,
                       webapps::WebappUninstallSource uninstall_source,
                       gfx::NativeWindow parent_window,
                       Callback callback);

 private:
  void OnWebAppUninstallDialogClosed(WebAppUninstallDialog* dialog,
                                     Callback callback,
                                     webapps::UninstallResultCode code);

  // All owned dialogs, running in parallel.
  base::flat_set<std::unique_ptr<WebAppUninstallDialog>,
                 base::UniquePtrComparator>
      dialogs_;

  const raw_ptr<Profile> profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_MANAGER_H_
