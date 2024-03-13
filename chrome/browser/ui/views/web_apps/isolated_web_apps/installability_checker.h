// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_INSTALLABILITY_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_INSTALLABILITY_CHECKER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class Profile;

namespace base {
class Version;
}  // namespace base

namespace web_app {

class IsolatedWebAppUrlInfo;
class IwaSourceBundleWithMode;
class SignedWebBundleMetadata;
class WebAppProvider;

class InstallabilityChecker {
 public:
  struct ProfileShutdown {};
  struct BundleInvalid {
    std::string error;
  };
  struct BundleInstallable {
    SignedWebBundleMetadata metadata;
  };
  struct BundleUpdatable {
    SignedWebBundleMetadata metadata;
    base::Version installed_version;
  };
  struct BundleOutdated {
    SignedWebBundleMetadata metadata;
    base::Version installed_version;
  };
  using Result = absl::variant<ProfileShutdown,
                               BundleInvalid,
                               BundleInstallable,
                               BundleUpdatable,
                               BundleOutdated>;

  static std::unique_ptr<InstallabilityChecker> CreateAndStart(
      Profile* profile,
      WebAppProvider* web_app_provider,
      IwaSourceBundleWithMode source,
      base::OnceCallback<void(Result)> callback);

  ~InstallabilityChecker();

 private:
  InstallabilityChecker(Profile* profile,
                        WebAppProvider* web_app_provider,
                        base::OnceCallback<void(Result)> callback);

  void Start(IwaSourceBundleWithMode source);
  void OnLoadedUrlInfo(
      IwaSourceBundleWithMode source,
      base::expected<IsolatedWebAppUrlInfo, std::string> url_info);
  void OnLoadedMetadata(
      base::expected<SignedWebBundleMetadata, std::string> metadata);
  void OnInstallabilityChecked(
      SignedWebBundleMetadata metadata,
      IsolatedInstallabilityCheckResult installability_check_result,
      std::optional<base::Version> installed_version);

  raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> web_app_provider_;
  base::OnceCallback<void(Result)> callback_;
  base::WeakPtrFactory<InstallabilityChecker> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_INSTALLABILITY_CHECKER_H_
