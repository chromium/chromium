// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ISOLATION_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ISOLATION_DELEGATE_H_

#include <memory>
#include <unordered_set>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/webapps/common/web_app_id.h"

class BrowserProcessImpl;
class Profile;
class TestingBrowserProcess;

namespace ui {
class UnownedUserDataHost;
}  // namespace ui

namespace web_app {

class ComputeAppSizeJob;
class WebAppProvider;

// Delegate owned by `WebAppProvider` that encapsulates Isolated Web App (IWA)
// lifecycle operations and subsystems, decoupling the core `:web_applications`
// target from `//chrome/browser/web_applications/isolated_web_apps`.
//
// Lifecycle contract with `WebAppProvider`:
// 1. Constructed via `Create()` during `WebAppProvider::CreateSubsystems()`.
//    Implementations register decoupled subsystems (such as the IWA managers)
//    onto `WebAppProvider::unowned_user_data_host()` via
//    `ui::ScopedUnownedUserData` for `<Subsystem>::From(provider)` access.
// 2. Wired to `WebAppProvider` via `SetProvider()` during
//    `WebAppProvider::ConnectSubsystems()`.
// 3. Started via `Start()` in `WebAppProvider::OnSyncBridgeReady()` after the
//    registry is ready and prior to `WebAppCommandManager::Start()`.
// 4. Torn down via `Shutdown()` during `WebAppProvider::Shutdown()`, after
//    `WebAppCommandManager::Shutdown()`.
class WebAppIsolationDelegate {
 public:
  using FactoryCallback = base::RepeatingCallback<std::unique_ptr<
      WebAppIsolationDelegate>(Profile*, ui::UnownedUserDataHost&)>;

  static void RegisterFactory(
      base::PassKey<BrowserProcessImpl, TestingBrowserProcess> pass_key,
      FactoryCallback factory);

  static std::unique_ptr<WebAppIsolationDelegate> Create(
      base::PassKey<WebAppProvider> pass_key,
      Profile* profile,
      ui::UnownedUserDataHost& unowned_user_data_host);

  WebAppIsolationDelegate();
  WebAppIsolationDelegate(const WebAppIsolationDelegate&) = delete;
  WebAppIsolationDelegate& operator=(const WebAppIsolationDelegate&) = delete;
  virtual ~WebAppIsolationDelegate();

  virtual void SetProvider(base::PassKey<WebAppProvider>,
                           WebAppProvider& provider) = 0;
  virtual void Start() = 0;
  virtual void Shutdown() = 0;

  // Called to clean up Isolated Web App specific resources when the app is
  // uninstalled (e.g. storage partitions and bundle caches).
  virtual void ClearAppResourcesOnUninstall(const webapps::AppId& app_id,
                                            base::OnceClosure callback) = 0;

  virtual std::unique_ptr<ComputeAppSizeJob> CreateComputeAppSizeJob(
      const webapps::AppId& app_id,
      base::DictValue& debug_value) = 0;

  // Returns a set of storage partition paths that are currently used by any
  // Isolated Web Apps installed in this profile; these paths are exempt from
  // garbage collection.
  virtual std::unordered_set<base::FilePath> GetIsolatedStoragePaths() = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ISOLATION_DELEGATE_H_
