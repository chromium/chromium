// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

class Profile;

namespace web_app {

class WebAppProvider;

class IsolatedWebAppUpdateManager {
 public:
  explicit IsolatedWebAppUpdateManager(Profile& profile);
  ~IsolatedWebAppUpdateManager();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Start();

  void Shutdown();

  base::Value AsDebugValue() const;

  void SetEnableAutomaticUpdatesForTesting(bool automatic_updates_enabled);

 private:
  raw_ref<Profile> profile_;
  bool automatic_updates_enabled_;

  raw_ptr<WebAppProvider> provider_ = nullptr;

  bool has_started_ = false;

  base::WeakPtrFactory<IsolatedWebAppUpdateManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_
