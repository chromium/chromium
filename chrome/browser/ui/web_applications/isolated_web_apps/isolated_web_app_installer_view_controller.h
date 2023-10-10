// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/web_applications/isolated_web_apps/isolated_web_app_installer_view.h"

namespace views {
class DialogDelegate;
class View;
}  // namespace views

namespace web_app {

class IsolatedWebAppInstallerModel;
class WebAppProvider;

class IsolatedWebAppInstallerViewController
    : public IsolatedWebAppInstallerView::Delegate {
 public:
  IsolatedWebAppInstallerViewController(WebAppProvider* web_app_provider,
                                        IsolatedWebAppInstallerModel* model);
  virtual ~IsolatedWebAppInstallerViewController();

  void Show(base::OnceClosure callback);

 private:
  // Handles returning a default value if the controller has been deleted.
  static bool OnAcceptWrapper(
      base::WeakPtr<IsolatedWebAppInstallerViewController> controller);

  bool OnAccept();
  void OnComplete();

  // Updates the View to reflect the current state of the model.
  void UpdateView();

  // `IsolatedWebAppInstallerView::Delegate`:
  void OnSettingsLinkClicked() override;

  std::unique_ptr<views::DialogDelegate> CreateDialogDelegate(
      std::unique_ptr<views::View> contents_view);

  raw_ptr<WebAppProvider> web_app_provider_;
  raw_ptr<IsolatedWebAppInstallerModel> model_;
  raw_ptr<IsolatedWebAppInstallerView> view_;
  raw_ptr<views::DialogDelegate> dialog_delegate_;

  base::OnceClosure callback_;
  base::WeakPtrFactory<IsolatedWebAppInstallerViewController> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_
