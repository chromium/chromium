// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_handler_utils.h"

#include "base/check.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_metrics_recorder.h"
#include "chromeos/ash/components/supervised_user/supervised_user_service_provider.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
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
  session_manager::SessionManager::Get()->RequestSignOut();
}

bool EnrollmentCompleted() {
  // EnrollmentCompleted() only runs from the Add Supervision dialog, which is
  // always shown inside the primary user's session, so a primary session
  // always exists here.
  // TODO(crbug.com/332804822): Take the AccountId from the callers instead of
  // resolving the primary session here.
  const session_manager::Session* session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  CHECK(session);
  supervised_user::SupervisedUserService* service =
      ash::SupervisedUserServiceProvider::Get().Find(session->account_id());
  return service && service->signout_required_after_supervision_enabled();
}
