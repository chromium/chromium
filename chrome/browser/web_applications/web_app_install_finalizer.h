// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_data.h"
#endif

class Profile;

namespace web_app {

class WebApp;
class FinalizeUpdateJob;
class WebAppProvider;
class WithAppResources;

// An finalizer for the installation process, represents the last step.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers an app.
class WebAppInstallFinalizer {
 public:

  explicit WebAppInstallFinalizer(Profile* profile);
  WebAppInstallFinalizer(const WebAppInstallFinalizer&) = delete;
  WebAppInstallFinalizer& operator=(const WebAppInstallFinalizer&) = delete;
  virtual ~WebAppInstallFinalizer();

  // Write the WebApp data to disk and register the app.
  // TODO(https://crbug.com/445700226): Move to a job, and remove copies.
  void FinalizeInstall(const WebAppInstallInfo& web_app_info,
                       const FinalizeJobOptions& options,
                       InstallFinalizedCallback callback);

  // Write the new WebApp data to disk and update the app.
  // TODO(crbug.com/40759394): Chrome fails to update the manifest
  // if the app window needing update closes at the same time as Chrome.
  // Therefore, the manifest may not always update as expected.
  // Virtual for testing.
  // TODO(https://crbug.com/445700226): Move to a job, and remove copies.
  virtual void FinalizeUpdate(const WebAppInstallInfo& web_app_info,
                              InstallFinalizedCallback callback);
  virtual void FinalizeUpdate(WithAppResources* lock,
                              const WebAppInstallInfo& web_app_info,
                              InstallFinalizedCallback callback);

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();
  void Shutdown();

  Profile* profile() { return profile_; }

  void SetClockForTesting(base::Clock* clock);

  base::WeakPtr<WebAppInstallFinalizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class FinalizeInstallJob;
  friend class FinalizeUpdateJob;

  using CommitCallback = base::OnceCallback<void(bool success)>;

  void OnInstallJobFinished(FinalizeInstallJob* job,
                            InstallFinalizedCallback callback,
                            const webapps::AppId& app_id,
                            webapps::InstallResultCode code);

  void OnInstallUpdateJobFinished(FinalizeUpdateJob* job,
                                  InstallFinalizedCallback callback,
                                  const webapps::AppId& app_id,
                                  webapps::InstallResultCode code);

  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  absl::flat_hash_set<std::unique_ptr<FinalizeInstallJob>> install_jobs_;
  absl::flat_hash_set<std::unique_ptr<FinalizeUpdateJob>> install_update_jobs_;

  bool started_ = false;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
