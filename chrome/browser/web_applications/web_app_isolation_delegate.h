// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ISOLATION_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ISOLATION_DELEGATE_H_

#include <memory>
#include <unordered_set>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/webapps/common/web_app_id.h"

class BrowserProcessImpl;
class Profile;
class TestingBrowserProcess;

namespace web_app {

class ComputeAppSizeJob;
class ComputedAppSizeWithOrigin;
class WebAppProvider;

// A delegate used by the WebAppProvider and command system to execute
// logic specific to Isolated Web Apps. By using a delegate, we decouple
// the core PWA system from the IWA implementations.
class WebAppIsolationDelegate {
 public:
  using FactoryCallback =
      base::RepeatingCallback<std::unique_ptr<WebAppIsolationDelegate>(
          Profile*)>;

  static void RegisterFactory(
      base::PassKey<BrowserProcessImpl, TestingBrowserProcess> pass_key,
      FactoryCallback factory);

  static std::unique_ptr<WebAppIsolationDelegate> Create(
      base::PassKey<WebAppProvider> pass_key,
      Profile* profile);

  virtual ~WebAppIsolationDelegate() = default;

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
