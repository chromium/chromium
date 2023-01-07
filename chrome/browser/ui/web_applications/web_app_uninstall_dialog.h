// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UNINSTALL_DIALOG_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}

namespace web_app {

// The interface of the uninstall dialog for web apps.
class WebAppUninstallDialog {
 public:
  WebAppUninstallDialog(const WebAppUninstallDialog&) = delete;
  WebAppUninstallDialog& operator=(const WebAppUninstallDialog&) = delete;

  virtual ~WebAppUninstallDialog() = default;

  // Create an implementation of WebAppUninstallDialog. The
  // dialog will be modal to |parent|, or a non-modal dialog if |parent| is
  // nullptr.
  static std::unique_ptr<WebAppUninstallDialog> Create(
      Profile* profile,
      gfx::NativeWindow parent);

  // Called when the dialog closes.
  using OnWebAppUninstallDialogClosed =
      base::OnceCallback<void(webapps::UninstallResultCode code)>;

  // Starts the process of showing a confirmation UI.
  virtual void ConfirmUninstall(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      OnWebAppUninstallDialogClosed closed_callback) = 0;

  virtual void SetDialogShownCallbackForTesting(base::OnceClosure callback) = 0;

 protected:
  WebAppUninstallDialog() = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UNINSTALL_DIALOG_H_
