// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_UNINSTALL_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_UNINSTALL_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

namespace web_app {

class WebAppUninstallWaiter final : public AppRegistrarObserver {
 public:
  WebAppUninstallWaiter(Profile* profile, AppId app_id);
  ~WebAppUninstallWaiter() final;
  void Wait();

  // AppRegistrarObserver:
  void OnWebAppUninstalled(const AppId& app_id) final;

 private:
  AppId app_id_;
  base::RunLoop run_loop_;
  ScopedObserver<AppRegistrar, AppRegistrarObserver> observer_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_UNINSTALL_WAITER_H_
