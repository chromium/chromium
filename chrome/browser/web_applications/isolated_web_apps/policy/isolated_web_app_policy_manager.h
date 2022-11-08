// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"

namespace web_app {

// This component is responsible for installing, uninstalling, updating etc.
// of the policy installed IWAs.
class IsolatedWebAppPolicyManager {
 public:
  enum class EphemeralAppInstallResult {
    kSuccess,
    kErrorNotEphemeralSession,
    kErrorCantCreateRootDirectory,
  };
  static constexpr char kEphemeralIwaRootDirectory[] = "EphemeralIWA";

  IsolatedWebAppPolicyManager(
      const base::FilePath& context_dir,
      std::vector<IsolatedWebAppExternalInstallOptions>&&
          ephemeral_iwa_install_options,
      base::OnceCallback<void(EphemeralAppInstallResult)> ephemeral_install_cb);
  ~IsolatedWebAppPolicyManager();

  // Triggers installing of the IWAs in MGS. There is no callback as so far we
  // don't care about the result of the installation: for MVP it is not critical
  // to have a complex retry mechanism for the session that would exist for just
  // several minutes.
  void InstallEphemeralApps();

  IsolatedWebAppPolicyManager(const IsolatedWebAppPolicyManager&) = delete;
  IsolatedWebAppPolicyManager& operator=(const IsolatedWebAppPolicyManager&) =
      delete;

 private:
  void CreateIwaEphemeralRootDirectory();
  void OnIwaEphemeralRootDirectoryCreated(base::File::Error error);

  // Isolated Web Apps for installation in ephemeral managed guest session.
  const std::vector<IsolatedWebAppExternalInstallOptions>
      ephemeral_iwa_install_options_;
  const base::FilePath installation_dir_;
  base::OnceCallback<void(EphemeralAppInstallResult)> ephemeral_install_cb_;
  base::WeakPtrFactory<IsolatedWebAppPolicyManager> weak_factory_{this};
};

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
