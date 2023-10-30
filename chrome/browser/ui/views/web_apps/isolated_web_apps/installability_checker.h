// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_INSTALLABILITY_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_INSTALLABILITY_CHECKER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace web_app {

class IsolatedWebAppUrlInfo;
class SignedWebBundleMetadata;
class WebAppProvider;

class InstallabilityChecker {
 public:
  class Delegate {
   public:
    virtual void OnProfileShutdown() = 0;
    virtual void OnBundleInvalid(const std::string& error) = 0;
    virtual void OnBundleInstallable(
        const SignedWebBundleMetadata& metadata) = 0;
    virtual void OnBundleUpdatable(const SignedWebBundleMetadata& metadata,
                                   const base::Version& installed_version) = 0;
    virtual void OnBundleOutdated(const SignedWebBundleMetadata& metadata,
                                  const base::Version& installed_version) = 0;
  };

  static std::unique_ptr<InstallabilityChecker> CreateAndStart(
      Profile* profile,
      WebAppProvider* web_app_provider,
      const base::FilePath& bundle_path,
      Delegate* delegate);

  ~InstallabilityChecker();

 private:
  InstallabilityChecker(Profile* profile,
                        WebAppProvider* web_app_provider,
                        Delegate* delegate);

  void Start(const base::FilePath& bundle_path);
  void OnLoadedUrlInfo(
      IsolatedWebAppLocation location,
      base::expected<IsolatedWebAppUrlInfo, std::string> url_info);
  void OnLoadedMetadata(
      base::expected<SignedWebBundleMetadata, std::string> metadata);
  void OnInstallabilityChecked(
      SignedWebBundleMetadata metadata,
      CheckIsolatedWebAppBundleInstallabilityCommand::InstallabilityCheckResult
          installability_check_result,
      absl::optional<base::Version> installed_version);

  raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> web_app_provider_;
  raw_ptr<Delegate> delegate_;
  base::WeakPtrFactory<InstallabilityChecker> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_INSTALLABILITY_CHECKER_H_
