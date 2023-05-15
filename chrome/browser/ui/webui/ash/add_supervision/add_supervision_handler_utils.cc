// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_handler_utils.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_metrics_recorder.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"

bool ShouldIncludeAppUpdate(const apps::AppUpdate& app_update) {
  return app_update.AppType() == apps::AppType::kArc &&
         app_update.InstallReason() != apps::InstallReason::kSystem;
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
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(
          ProfileManager::GetPrimaryUserProfile());
  return service->signout_required_after_supervision_enabled();
}
