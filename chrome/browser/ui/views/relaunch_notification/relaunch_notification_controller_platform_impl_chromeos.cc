// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller_platform_impl_chromeos.h"

#include "ash/public/cpp/update_types.h"
#include "base/bind.h"
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
    relaunch_notification::RecordRecommendedShowResult(
        relaunch_notification::ShowResult::kShown);
    recorded_shown_ = true;
  }
}

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRequired(
    base::Time deadline) {
  if (!relaunch_required_timer_) {
    relaunch_required_timer_ = std::make_unique<RelaunchRequiredTimer>(
        deadline,
        base::BindRepeating(&RelaunchNotificationControllerPlatformImpl::
                                RefreshRelaunchRequiredTitle,
                            base::Unretained(this)));

    relaunch_notification::RecordRequiredShowResult(
        relaunch_notification::ShowResult::kShown);
  }

  RefreshRelaunchRequiredTitle();
}

void RelaunchNotificationControllerPlatformImpl::CloseRelaunchNotification() {
  SystemTrayClient::Get()->SetUpdateNotificationState(
      ash::NotificationStyle::kDefault, base::string16(), base::string16());
  recorded_shown_ = false;
  relaunch_required_timer_.reset();
}

void RelaunchNotificationControllerPlatformImpl::SetDeadline(
    base::Time deadline) {
  relaunch_required_timer_->SetDeadline(deadline);
}

void RelaunchNotificationControllerPlatformImpl::
    RefreshRelaunchRecommendedTitle(bool past_deadline) {
  if (past_deadline) {
    SystemTrayClient::Get()->SetUpdateNotificationState(
        ash::NotificationStyle::kAdminRecommended,
        l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_OVERDUE_TITLE),
        l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_OVERDUE_BODY));
  } else {
    SystemTrayClient::Get()->SetUpdateNotificationState(
        ash::NotificationStyle::kAdminRecommended,
        l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_TITLE),
        l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_BODY));
  }
}

void RelaunchNotificationControllerPlatformImpl::
    RefreshRelaunchRequiredTitle() {
  SystemTrayClient::Get()->SetUpdateNotificationState(
      ash::NotificationStyle::kAdminRequired,
      relaunch_required_timer_->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_RELAUNCH_REQUIRED_BODY));
}

bool RelaunchNotificationControllerPlatformImpl::IsRequiredNotificationShown()
    const {
  return relaunch_required_timer_ != nullptr;
}
