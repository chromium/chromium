// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller_platform_impl_chromeos.h"

#include "ash/public/cpp/update_types.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_metrics.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

RelaunchNotificationControllerPlatformImpl::
    RelaunchNotificationControllerPlatformImpl() = default;

RelaunchNotificationControllerPlatformImpl::
    ~RelaunchNotificationControllerPlatformImpl() = default;

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRecommended(
    base::Time /*detection_time*/,
    bool past_deadline) {
  RecordRecommendedShowResult();
  RefreshRelaunchRecommendedTitle(past_deadline);
}

void RelaunchNotificationControllerPlatformImpl::RecordRecommendedShowResult() {
  if (!recorded_shown_) {
    relaunch_notification::RecordRecommendedShowResult();
    recorded_shown_ = true;
  }
}

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRequired(
    base::Time deadline,
    base::OnceCallback<base::Time()> on_visible) {
  if (!relaunch_required_timer_) {
    relaunch_required_timer_ = std::make_unique<RelaunchRequiredTimer>(
        deadline,
        base::BindRepeating(&RelaunchNotificationControllerPlatformImpl::
                                RefreshRelaunchRequiredTitle,
                            base::Unretained(this)));

    relaunch_notification::RecordRequiredShowResult();
  }

  RefreshRelaunchRequiredTitle();

  if (!CanScheduleReboot()) {
    on_visible_ = std::move(on_visible);
    StartObserving();
  } else {
    StopObserving();
  }
}

void RelaunchNotificationControllerPlatformImpl::CloseRelaunchNotification() {
  SystemTrayClient::Get()->SetUpdateNotificationState(
      ash::NotificationStyle::kDefault, base::string16(), base::string16());
  recorded_shown_ = false;
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
  std::string enterprise_display_domain =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetEnterpriseDisplayDomain();
  if (past_deadline) {
    SystemTrayClient::Get()->SetUpdateNotificationState(
        ash::NotificationStyle::kAdminRecommended,
        l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_OVERDUE_TITLE),
        l10n_util::GetStringFUTF16(
            IDS_RELAUNCH_RECOMMENDED_OVERDUE_BODY,
            base::UTF8ToUTF16(enterprise_display_domain)));
  } else {
    SystemTrayClient::Get()->SetUpdateNotificationState(
        ash::NotificationStyle::kAdminRecommended,
        l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_TITLE),
        l10n_util::GetStringFUTF16(
            IDS_RELAUNCH_RECOMMENDED_BODY,
            base::UTF8ToUTF16(enterprise_display_domain)));
  }
}

bool RelaunchNotificationControllerPlatformImpl::IsRequiredNotificationShown()
    const {
  return relaunch_required_timer_ != nullptr;
}

void RelaunchNotificationControllerPlatformImpl::
    RefreshRelaunchRequiredTitle() {
  // SystemTrayClient may not exist in unit tests.
  if (SystemTrayClient::Get()) {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    SystemTrayClient::Get()->SetUpdateNotificationState(
        ash::NotificationStyle::kAdminRequired,
        relaunch_required_timer_->GetWindowTitle(),
        l10n_util::GetStringFUTF16(
            IDS_RELAUNCH_REQUIRED_BODY,
            base::UTF8ToUTF16(connector->GetEnterpriseDisplayDomain())));
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
  if (!display_observer_.IsObservingSources())
    display_observer_.Add(ash::Shell::Get()->display_configurator());
  if (!session_observer_.IsObservingSources())
    session_observer_.Add(session_manager::SessionManager::Get());
}

void RelaunchNotificationControllerPlatformImpl::StopObserving() {
  display_observer_.RemoveAll();
  session_observer_.RemoveAll();
}
