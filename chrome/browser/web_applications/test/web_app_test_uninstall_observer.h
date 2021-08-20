// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UNINSTALL_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UNINSTALL_OBSERVER_H_

#include <set>

#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/test/web_app_test_registry_observer_adapter.h"

class Profile;

namespace web_app {

class WebAppTestUninstallObserver final
    : public WebAppTestRegistryObserverAdapter {
 public:
  explicit WebAppTestUninstallObserver(
      Profile* profile,
      const std::set<AppId>& listening_for_uninstall_app_ids = {});
  ~WebAppTestUninstallObserver() final;
  AppId Wait();

  // AppRegistrarObserver:
  void OnWebAppUninstalled(const AppId& app_id) final;

 private:
  std::set<AppId> listening_for_uninstall_app_ids_;
  base::OnceCallback<void(const AppId& app_id)> app_uninstalled_delegate_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UNINSTALL_OBSERVER_H_
