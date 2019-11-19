// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler_utils.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_metrics_recorder.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

bool ShouldIncludeAppUpdate(const apps::AppUpdate& app_update) {
  // TODO(danan): update this to only return sticky = true arc apps when that
  // attribute is available via the App Service (https://crbug.com/948408).

  return app_update.AppType() == apps::mojom::AppType::kArc;
}

void LogOutHelper() {
  // Record UMA metric that the user clicked "Sign out".
  if (EnrollmentCompleted()) {
    AddSupervisionMetricsRecorder::GetInstance()
        ->RecordAddSupervisionEnrollment(
            AddSupervisionMetricsRecorder::EnrollmentState::kSignedOut);
  } else {
    AddSupervisionMetricsRecorder::GetInstance()
        ->RecordAddSupervisionEnrollment(
            AddSupervisionMetricsRecorder::EnrollmentState::kSwitchedAccounts);
  }
  chrome::AttemptUserExit();
}

bool EnrollmentCompleted() {
  SupervisedUserService* service = SupervisedUserServiceFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
  return service->signout_required_after_supervision_enabled();
}
