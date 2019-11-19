// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class AppRegistrar;

class WebAppInstallObserver final : public AppRegistrarObserver {
 public:
  explicit WebAppInstallObserver(AppRegistrar* registrar);
  explicit WebAppInstallObserver(Profile* profile);
  ~WebAppInstallObserver() override;

  AppId AwaitNextInstall();

  using WebAppUninstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppUninstalledDelegate(WebAppUninstalledDelegate delegate);

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppUninstalled(const AppId& app_id) override;

 private:
  base::RunLoop run_loop_;
  AppId app_id_;

  WebAppUninstalledDelegate app_uninstalled_delegate_;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallObserver);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_
