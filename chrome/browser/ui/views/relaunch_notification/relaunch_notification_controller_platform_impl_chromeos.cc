// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller_platform_impl_chromeos.h"

#include <utility>

#include "ash/public/cpp/update_types.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/display/manager/display_configurator.h"

RelaunchNotificationControllerPlatformImpl::
    RelaunchNotificationControllerPlatformImpl() = default;

RelaunchNotificationControllerPlatformImpl::
    ~RelaunchNotificationControllerPlatformImpl() = default;

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRecommended(
    base::Time /*detection_time*/,
    bool past_deadline) {
  RefreshRelaunchRecommendedTitle(past_deadline);
}

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRequired(
    base::Time deadline,
    bool is_notification_type_overriden,
    base::OnceCallback<base::Time()> on_visible) {
  if (!relaunch_required_timer_) {
    relaunch_required_timer_ = std::make_unique<RelaunchRequiredTimer>(
        deadline, base::BindRepeating(
                      &RelaunchNotificationControllerPlatformImpl::
                          RefreshRelaunchRequiredTitle,
                      base::Unretained(this), is_notification_type_overriden));
  }

  RefreshRelaunchRequiredTitle(is_notification_type_overriden);

  if (!CanScheduleReboot()) {
    on_visible_ = std::move(on_visible);
    StartObserving();
  } else {
    StopObserving();
  }
}

void RelaunchNotificationControllerPlatformImpl::CloseRelaunchNotification() {
  SystemTrayClientImpl::Get()->ResetUpdateState();
  relaunch_required_timer_.reset();
  on_visible_.Reset();
  StopObserving();
}

void RelaunchNotificationControllerPlatformImpl::SetDeadline(
    base::Time deadline) {
  if (relaunch_required_timer_)
    relaunch_required_timer_->SetDeadline(deadline);
}

void RelaunchNotificationControllerPlatformImpl::
    RefreshRelaunchRecommendedTitle(bool past_deadline) {
  if (past_deadline) {
    SystemTrayClientImpl::Get()->SetRelaunchNotificationState(
        {.requirement_type =
             ash::RelaunchNotificationState::kRecommendedAndOverdue});
  } else {
    SystemTrayClientImpl::Get()->SetRelaunchNotificationState(
        {.requirement_type =
             ash::RelaunchNotificationState::kRecommendedNotOverdue});
  }
}

bool RelaunchNotificationControllerPlatformImpl::IsRequiredNotificationShown()
    const {
  return relaunch_required_timer_ != nullptr;
}

void RelaunchNotificationControllerPlatformImpl::RefreshRelaunchRequiredTitle(
    bool is_notification_type_overriden) {
  // SystemTrayClientImpl may not exist in unit tests.
  if (SystemTrayClientImpl::Get()) {
    SystemTrayClientImpl::Get()->SetRelaunchNotificationState(
        {.requirement_type = ash::RelaunchNotificationState::kRequired,
         // We only override notification type to kRequired in the
         // MinimumVersionPolicyHandler that handles device policies.
         .policy_source = is_notification_type_overriden
                              ? ash::RelaunchNotificationState::kDevice
                              : ash::RelaunchNotificationState::kUser,
         .rounded_time_until_reboot_required =
             relaunch_required_timer_->GetRoundedDeadlineDelta()});
  }
}

void RelaunchNotificationControllerPlatformImpl::OnPowerStateChanged(
    chromeos::DisplayPowerState power_state) {
  if (CanScheduleReboot() && on_visible_) {
    base::Time new_deadline = std::move(on_visible_).Run();
    SetDeadline(new_deadline);
    StopObserving();
  }
}

void RelaunchNotificationControllerPlatformImpl::OnSessionStateChanged() {
  TRACE_EVENT0(
      "login",
      "RelaunchNotificationControllerPlatformImpl::OnSessionStateChanged");
  if (CanScheduleReboot() && on_visible_) {
    base::Time new_deadline = std::move(on_visible_).Run();
    SetDeadline(new_deadline);
    StopObserving();
  }
}

bool RelaunchNotificationControllerPlatformImpl::CanScheduleReboot() {
  return ash::Shell::Get()->display_configurator()->IsDisplayOn() &&
         session_manager::SessionManager::Get()->session_state() ==
             session_manager::SessionState::ACTIVE;
}

void RelaunchNotificationControllerPlatformImpl::StartObserving() {
  if (!display_observation_.IsObserving())
    display_observation_.Observe(ash::Shell::Get()->display_configurator());
  if (!session_observation_.IsObserving())
    session_observation_.Observe(session_manager::SessionManager::Get());
}

void RelaunchNotificationControllerPlatformImpl::StopObserving() {
  display_observation_.Reset();
  session_observation_.Reset();
}
