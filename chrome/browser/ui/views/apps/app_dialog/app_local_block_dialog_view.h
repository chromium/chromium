// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_LOCAL_BLOCK_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_LOCAL_BLOCK_DIALOG_VIEW_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"

// The blocking dialog for the app blocked by local settings.
class AppLocalBlockDialogView : public AppDialogView {
 public:
  explicit AppLocalBlockDialogView(const std::string& app_name);
  ~AppLocalBlockDialogView() override;

  static AppLocalBlockDialogView* GetActiveViewForTesting();

  // Add a new blocked app to be shown on the dialog.
  void AddApp(const std::string& app_name);

 private:
  std::vector<std::string> app_names_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_LOCAL_BLOCK_DIALOG_VIEW_H_
