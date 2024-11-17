// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_PREPARE_INSTALL_INFO_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_PREPARE_INSTALL_INFO_JOB_H_

#include <type_traits>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

class Profile;

namespace web_app {

class IsolatedWebAppInstallCommandHelper;

// Loads the manifest from the bundle and assembles `WebAppInstallInfo`.
class PrepareInstallInfoJob {
 public:
  enum class Error {
    kCantLoadInstallUrl = 1,
    kAppIsNotInstallable = 2,
    kCantValidateManifest = 3,
    kCantRetrieveIcons = 4,
  };

  struct Failure {
    Error error;
    std::string message;
  };

  using InstallInfoOrFailure = base::expected<WebAppInstallInfo, Failure>;
  using ResultCallback = base::OnceCallback<void(InstallInfoOrFailure)>;

  static std::unique_ptr<PrepareInstallInfoJob> CreateAndStart(
      Profile& profile,
      IwaSourceWithMode source,
      std::optional<base::Version> expected_version,
      content::WebContents& web_contents,
      IsolatedWebAppInstallCommandHelper& command_helper,
      std::unique_ptr<webapps::WebAppUrlLoader> loader,
      ResultCallback callback);

  ~PrepareInstallInfoJob();

  PrepareInstallInfoJob(const PrepareInstallInfoJob&) = delete;
  PrepareInstallInfoJob& operator=(const PrepareInstallInfoJob&) = delete;

 private:
  template <typename T>
    requires(std::is_void_v<T>)
  void RunNextStepOnSuccess(base::OnceClosure next_step_callback,
                            Error error,
                            base::expected<T, std::string> status) {
    if (!status.has_value()) {
      ReportFailure(error, status.error());
    } else {
      std::move(next_step_callback).Run();
    }
  }

  template <typename T>
    requires(!std::is_void_v<T>)
  void RunNextStepOnSuccess(base::OnceCallback<void(T)> next_step_callback,
                            Error error,
                            base::expected<T, std::string> status) {
    if (!status.has_value()) {
      ReportFailure(error, status.error());
    } else {
      std::move(next_step_callback).Run(std::move(*status));
    }
  }

  PrepareInstallInfoJob(Profile& profile,
                        IwaSourceWithMode source,
                        std::optional<base::Version> expected_version,
                        content::WebContents& web_contents,
                        IsolatedWebAppInstallCommandHelper& command_helper);

  void Start(std::unique_ptr<webapps::WebAppUrlLoader> loader,
             ResultCallback callback);

  void ReportFailure(Error error, const std::string& message);

  Profile* profile() { return &*profile_; }

  void LoadInstallUrl(base::OnceClosure next_step_callback);

  void CheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback);

  void ValidateManifestAndCreateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      blink::mojom::ManifestPtr manifest);

  void RetrieveIconsAndPopulateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      WebAppInstallInfo install_info);

  void FinishJob(WebAppInstallInfo info);

  const raw_ref<Profile> profile_;

  const IwaSourceWithMode source_;
  const std::optional<base::Version> expected_version_;
  const raw_ref<content::WebContents> web_contents_;
  const raw_ref<IsolatedWebAppInstallCommandHelper> command_helper_;

  ResultCallback callback_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;

  base::WeakPtrFactory<PrepareInstallInfoJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_PREPARE_INSTALL_INFO_JOB_H_
