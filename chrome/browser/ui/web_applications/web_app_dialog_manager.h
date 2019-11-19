// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class BrowserWindow;
class Profile;

namespace web_app {

// An internal WebAppDialogManager's representation of any running dialog.
class DialogInstance;

class WebAppDialogManager {
 public:
  explicit WebAppDialogManager(Profile* profile);
  ~WebAppDialogManager();

  enum class UninstallSource {
    kAppMenu,
    kAppsPage,
  };

  using Callback = base::OnceCallback<void(bool success)>;

  bool CanUninstallWebApp(const AppId& app_id) const;
  // The uninstall dialog will be modal to |parent_window|, or a non-modal if
  // |parent_window| is nullptr.
  void UninstallWebApp(const AppId& app_id,
                       UninstallSource uninstall_source,
                       BrowserWindow* parent_window,
                       Callback callback);

 private:
  void OnDialogCompleted(DialogInstance* dialog,
                         Callback callback,
                         bool success);

  // All owned dialogs, running in parallel.
  base::flat_set<std::unique_ptr<DialogInstance>, base::UniquePtrComparator>
      dialogs_;

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(WebAppDialogManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_MANAGER_H_
