// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_SUB_APPS_INSTALL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_SUB_APPS_INSTALL_DIALOG_CONTROLLER_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/models/dialog_model.h"

namespace web_app {

// Controller class for the dialog created from `ShowSubAppsInstallDialog`.
class SubAppsInstallDialogController : public ui::DialogModelDelegate {
 public:
  enum class DialogActionForTesting { kAccept, kCancel };
  enum class SubAppsInstallDialogViewID : int {
    VIEW_ID_NONE = 0,
    SUB_APP_LABEL,
    SUB_APP_ICON,
    MANAGE_PERMISSIONS_LINK,
  };

  // Sets the `auto_accept` action to accept/cancel the dialog in tests.
  static base::AutoReset<std::optional<DialogActionForTesting>>
  SetAutomaticActionForTesting(DialogActionForTesting auto_accept);

  explicit SubAppsInstallDialogController(
      base::OnceCallback<void(bool)> callback);
  SubAppsInstallDialogController(const SubAppsInstallDialogController&) =
      delete;
  SubAppsInstallDialogController& operator=(
      const SubAppsInstallDialogController&) = delete;
  ~SubAppsInstallDialogController() override;

  // Used to accept/cancel the dialog in testing. Returns true if the dialog was
  // handled automatically.
  static bool HandleAutomaticActionForTesting(
      base::OnceCallback<void(bool)>& callback);

  // To be called when the dialog is accepted.
  void OnAccept();
  // To be called when the dialog is cancelled, dismissed, or destroyed.
  void OnClose();

  base::WeakPtr<SubAppsInstallDialogController> GetWeakPtr();

 private:
  base::OnceCallback<void(bool)> callback_;

  base::WeakPtrFactory<SubAppsInstallDialogController> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_SUB_APPS_INSTALL_DIALOG_CONTROLLER_H_
