// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ISOLATED_WEB_APP_INSTALLER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ISOLATED_WEB_APP_INSTALLER_CONTEXT_MENU_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "ui/base/models/simple_menu_model.h"

// A simple Ash shelf context menu for an Isolated Web App installer instance.
// Currently only supports "Close".
class IsolatedWebAppInstallerContextMenu
    : public ui::SimpleMenuModel::Delegate {
 public:
  explicit IsolatedWebAppInstallerContextMenu(base::OnceClosure close_closure);
  ~IsolatedWebAppInstallerContextMenu() override;

  IsolatedWebAppInstallerContextMenu(
      const IsolatedWebAppInstallerContextMenu&) = delete;
  IsolatedWebAppInstallerContextMenu& operator=(
      const IsolatedWebAppInstallerContextMenu&) = delete;

  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  void GetMenuModel(GetMenuModelCallback callback);

  // SimpleMenuModel::Delegate override:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // This callback closes the installer instance.
  base::OnceClosure close_closure_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ISOLATED_WEB_APP_INSTALLER_CONTEXT_MENU_H_
