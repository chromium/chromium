// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_APPLY_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_APPLY_WAITER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"

class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace web_app {

class WebAppUiManager;

// This class waits for the right time to apply a pending and prepared update
// for an IWA. It will keep profile and browser alive while waiting.
//
// For now, it will simply wait for all windows of the IWA to be closed. In
// the future, it might wait for more complex conditions, such as the user
// clicking a button to apply the update.
class IsolatedWebAppUpdateApplyWaiter {
 public:
  using Callback =
      base::OnceCallback<void(std::unique_ptr<ScopedKeepAlive>,
                              std::unique_ptr<ScopedProfileKeepAlive>)>;

  IsolatedWebAppUpdateApplyWaiter(IsolatedWebAppUrlInfo url_info,
                                  WebAppUiManager& ui_manager);
  ~IsolatedWebAppUpdateApplyWaiter();

  IsolatedWebAppUpdateApplyWaiter(const IsolatedWebAppUpdateApplyWaiter&) =
      delete;
  IsolatedWebAppUpdateApplyWaiter& operator=(
      const IsolatedWebAppUpdateApplyWaiter&) = delete;

  void Wait(Profile* profile, Callback callback);

  base::Value AsDebugValue() const;

 private:
  void Signal();

  IsolatedWebAppUrlInfo url_info_;
  raw_ref<WebAppUiManager> ui_manager_;

  Callback callback_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  base::WeakPtrFactory<IsolatedWebAppUpdateApplyWaiter> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_APPLY_WAITER_H_
