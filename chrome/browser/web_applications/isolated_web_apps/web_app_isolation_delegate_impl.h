// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WEB_APP_ISOLATION_DELEGATE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WEB_APP_ISOLATION_DELEGATE_IMPL_H_

#include <memory>
#include <unordered_set>

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_dev_install_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_user_installed_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/web_app_isolation_delegate.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.h"
#endif

class Profile;

namespace ui {
class UnownedUserDataHost;
}  // namespace ui

namespace web_app {

class WebAppIsolationDelegateImpl : public WebAppIsolationDelegate {
 public:
  static std::unique_ptr<WebAppIsolationDelegate> Create(
      Profile* profile,
      ui::UnownedUserDataHost& unowned_user_data_host);

  ~WebAppIsolationDelegateImpl() override;

  // WebAppIsolationDelegate:
  void SetProvider(base::PassKey<WebAppProvider>,
                   WebAppProvider& provider) override;
  void Start() override;
  void Shutdown() override;
  void ClearAppResourcesOnUninstall(const webapps::AppId& app_id,
                                    base::OnceClosure callback) override;
  std::unique_ptr<ComputeAppSizeJob> CreateComputeAppSizeJob(
      const webapps::AppId& app_id,
      base::DictValue& debug_value) override;
  std::unordered_set<base::FilePath> GetIsolatedStoragePaths() override;

 private:
  WebAppIsolationDelegateImpl(Profile* profile,
                              ui::UnownedUserDataHost& unowned_user_data_host);

  const raw_ref<Profile> profile_;
  IsolatedWebAppDevInstallManager dev_install_manager_;
  IsolatedWebAppPolicyManager policy_manager_;
  IsolatedWebAppUserInstalledManager user_installed_manager_;
  IsolatedWebAppUpdateManager update_manager_;
#if BUILDFLAG(IS_CHROMEOS)
  IwaBundleCacheManager cache_manager_;
#endif
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WEB_APP_ISOLATION_DELEGATE_IMPL_H_
