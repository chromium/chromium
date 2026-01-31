// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"

class Profile;

namespace web_app {

class WebAppProvider;

// A finalizer job for the installation process, represents the last step.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers the app.
//
// This is a job based on web_app_install_finalizer. At the moment it still
// depends on WebAppInstallFinalizer. When refactoring, this job will stand on
// its own.
class FinalizeInstallJob {
 public:
  FinalizeInstallJob(Profile& profile,
                     WebAppProvider& provider,
                     WebAppInstallFinalizer& finalizer,
                     const WebAppInstallInfo& web_app_info,
                     const WebAppInstallFinalizer::FinalizeOptions& options);
  ~FinalizeInstallJob();

  void Start(WebAppInstallFinalizer::InstallFinalizedCallback callback);

 private:
  friend class WebAppInstallFinalizer;

  void OnOriginAssociationValidated(
      OriginAssociations validated_origin_associations);

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppProvider> provider_;
  const raw_ref<WebAppInstallFinalizer> finalizer_;

  WebAppInstallInfo web_app_info_;
  WebAppInstallFinalizer::FinalizeOptions options_;
  WebAppInstallFinalizer::InstallFinalizedCallback callback_;

  base::WeakPtrFactory<FinalizeInstallJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_
