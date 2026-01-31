// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_USER_INSTALLED_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_USER_INSTALLED_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"

class Profile;

namespace web_app {

class WebAppProvider;

// This class manages the lifetime of isolated web app installed by user
// through Graphical User Interface.
// TODO(crbug.com/477039906): Move user-installation flow to go through this
// manager.
class IsolatedWebAppUserInstalledManager {
 public:
  explicit IsolatedWebAppUserInstalledManager(Profile& profile);
  ~IsolatedWebAppUserInstalledManager();

  void Start();
  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

 private:
  void OnRuntimeDataChanged();

  raw_ref<Profile> profile_;
  raw_ptr<WebAppProvider> provider_;
  base::CallbackListSubscription runtime_data_subscription_;

  base::WeakPtrFactory<IsolatedWebAppUserInstalledManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_USER_INSTALLED_MANAGER_H_
