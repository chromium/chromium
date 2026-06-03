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
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

class Profile;

namespace web_app {

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
      IwaOperation operation,
      std::optional<IwaVersion> expected_version,
      IsolatedWebAppUrlInfo url_info,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
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
                        IwaOperation operation,
                        std::optional<IwaVersion> expected_version,
                        IsolatedWebAppUrlInfo url_info,
                        std::unique_ptr<WebAppDataRetriever> data_retriever);

  void Start(std::unique_ptr<webapps::WebAppUrlLoader> loader,
             ResultCallback callback);

  void ReportFailure(Error error, const std::string& message);

  Profile* profile() { return &*profile_; }

  void LoadInstallUrl(base::OnceClosure next_step_callback);
  void OnLoadInstallUrl(base::OnceClosure next_step_callback,
                        webapps::WebAppUrlLoaderResult result);

  void CheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback);
  void OnCheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback,
      blink::mojom::ManifestPtr opt_manifest,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode error_code);

  void ValidateManifestAndGetVersion(
      base::OnceCallback<void(IwaVersion)> next_step_callback,
      blink::mojom::ManifestPtr manifest);

  void ParseInstallInfoFromManifest(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      const IwaVersion parsed_version);
  void OnGettingInstallInfoFromManifest(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      const IwaVersion parsed_version,
      std::unique_ptr<WebAppInstallInfo> install_info);

  void FinishJob(WebAppInstallInfo info);

  const raw_ref<Profile> profile_;

  const IwaSourceWithMode source_;
  const IwaOperation operation_;
  const std::optional<IwaVersion> expected_version_;
  IsolatedWebAppUrlInfo url_info_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;
  base::DictValue manifest_to_info_debug_data_;

  blink::mojom::ManifestPtr manifest_;

  ResultCallback callback_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;

  base::WeakPtrFactory<PrepareInstallInfoJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_PREPARE_INSTALL_INFO_JOB_H_
