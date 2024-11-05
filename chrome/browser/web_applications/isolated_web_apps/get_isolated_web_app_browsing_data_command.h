// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_ISOLATED_WEB_APP_BROWSING_DATA_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_ISOLATED_WEB_APP_BROWSING_DATA_COMMAND_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"

class Profile;

namespace url {
class Origin;
}  // namespace url

namespace web_app {

class GetIsolatedWebAppSizeJob;

struct GetIsolatedWebAppSizeJobResult;

// Computes the total browsing data usage in bytes of every installed Isolated
// Web App.
class GetIsolatedWebAppBrowsingDataCommand
    : public WebAppCommand<AllAppsLock, base::flat_map<url::Origin, int64_t>> {
 public:
  using BrowsingDataCallback =
      base::OnceCallback<void(base::flat_map<url::Origin, int64_t>)>;

  GetIsolatedWebAppBrowsingDataCommand(Profile* profile,
                                       BrowsingDataCallback callback);
  ~GetIsolatedWebAppBrowsingDataCommand() override;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  void CompleteCommand(
      std::vector<std::optional<GetIsolatedWebAppSizeJobResult>>
          app_size_results);

  const raw_ref<Profile> profile_;
  std::unique_ptr<AllAppsLock> lock_;
  std::vector<std::unique_ptr<GetIsolatedWebAppSizeJob>>
      get_isolated_web_app_size_jobs_;

  base::WeakPtrFactory<GetIsolatedWebAppBrowsingDataCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_ISOLATED_WEB_APP_BROWSING_DATA_COMMAND_H_
