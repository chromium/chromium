// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_handler.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/notifications/notification_display_service.h"  // nogncheck
#include "chrome/browser/notifications/notification_display_service_factory.h"  // nogncheck
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate.h"

namespace {

const int kReviewButtonIndex = 1;

}  // namespace

AnnouncementNotificationHandler::AnnouncementNotificationHandler() = default;

AnnouncementNotificationHandler::~AnnouncementNotificationHandler() = default;

void AnnouncementNotificationHandler::OnShow(
    Profile* profile,
    const std::string& notification_id) {
}

void AnnouncementNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  std::move(completed_closure).Run();
}

void AnnouncementNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  int button_index = action_index.has_value() ? action_index.value() : -1;

  // Open the announcement link when the user clicks the notification or clicks
  // the button to open.
  if (button_index == kReviewButtonIndex || !action_index.has_value()) {
    OpenAnnouncement(profile);
    std::move(completed_closure).Run();
    return;
  }

  // Otherwise, close the notification.
  NotificationDisplayServiceFactory::GetInstance()
      ->GetForProfile(profile)
      ->Close(NotificationHandler::Type::ANNOUNCEMENT,
              kAnnouncementNotificationId);
  std::move(completed_closure).Run();
}

void AnnouncementNotificationHandler::OpenAnnouncement(Profile* profile) {
  if (!AnnouncementNotificationService::CanOpenAnnouncement(profile))
    return;

  // Open the announcement URL in a new tab.
  GURL url = AnnouncementNotificationService::GetAnnouncementURL();
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);
}
