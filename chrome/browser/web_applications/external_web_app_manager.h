// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"

namespace base {
class FilePath;
}

class Profile;

namespace web_app {

class PendingAppManager;

class ExternalWebAppManager {
 public:
  static const char* kHistogramEnabledCount;
  static const char* kHistogramDisabledCount;
  static const char* kHistogramConfigErrorCount;

  explicit ExternalWebAppManager(Profile* profile);
  ~ExternalWebAppManager();

  void SetSubsystems(PendingAppManager* pending_app_manager);

  void Start();

  // Scans the given directory (non-recursively) for *.json files that define
  // "external web apps", the Web App analogs of "external extensions",
  // described at https://developer.chrome.com/apps/external_extensions
  //
  // This function performs file I/O, and must not be scheduled on UI threads.
  static std::vector<ExternalInstallOptions> ReloadInstallOptionsForTesting(
      std::unique_ptr<FileUtilsWrapper> file_utils,
      const base::FilePath& dir,
      Profile* profile);

  using LoadCallback =
      base::OnceCallback<void(std::vector<ExternalInstallOptions>)>;

  void LoadInstallOptions(LoadCallback callback);

  static void SkipStartupForTesting();

  void SynchronizeAppsForTesting(
      std::unique_ptr<FileUtilsWrapper> file_utils,
      std::vector<std::string> app_configs,
      PendingAppManager::SynchronizeCallback callback);

 private:
  void SynchronizeExternalInstallOptions(
      PendingAppManager::SynchronizeCallback callback,
      std::vector<ExternalInstallOptions>);

  PendingAppManager* pending_app_manager_ = nullptr;
  Profile* const profile_;

  base::WeakPtrFactory<ExternalWebAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExternalWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_
