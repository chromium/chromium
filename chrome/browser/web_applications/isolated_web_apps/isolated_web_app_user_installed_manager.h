// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_USER_INSTALLED_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_USER_INSTALLED_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"

class Profile;

namespace web_app {

class WebAppProvider;
class IsolatedWebAppUrlInfo;
class IsolatedWebAppInstallSource;
class IwaVersion;
struct InstallIsolatedWebAppCommandSuccess;
struct InstallIsolatedWebAppCommandError;

// This class manages the lifetime of isolated web app installed by user
// through Graphical User Interface.
class IsolatedWebAppUserInstalledManager {
  using InstallIsolatedWebAppCallback = base::OnceCallback<void(
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError>)>;

 public:
  explicit IsolatedWebAppUserInstalledManager(Profile& profile);
  ~IsolatedWebAppUserInstalledManager();

  void Start();
  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Install(const IsolatedWebAppUrlInfo& url_info,
               const IsolatedWebAppInstallSource& source,
               const std::optional<IwaVersion>& expected_version,
               InstallIsolatedWebAppCallback callback);

 private:
  void OnRuntimeDataChanged();

  raw_ref<Profile> profile_;
  raw_ptr<WebAppProvider> provider_;
  base::CallbackListSubscription runtime_data_subscription_;

  base::WeakPtrFactory<IsolatedWebAppUserInstalledManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_USER_INSTALLED_MANAGER_H_
