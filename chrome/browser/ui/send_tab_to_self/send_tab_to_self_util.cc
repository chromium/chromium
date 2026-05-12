// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/origin.h"

namespace send_tab_to_self {

namespace {

// Opens the given `entry` in a new tab with the given `disposition` for the
// given `profile`.
// Returns a weak pointer to the opened WebContents.
base::WeakPtr<content::WebContents> OpenEntryInNewTabWithDisposition(
    Profile* profile,
    const SendTabToSelfEntry& entry,
    WindowOpenDisposition disposition) {
  RecordHasScrollPositionOnOpened(
      !entry.GetPageContext().scroll_position.IsEmpty());

  NavigateParams params(profile, entry.GetURL(), ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  params.window_action = NavigateParams::WindowAction::kShowWindow;

  std::optional<std::string> scroll_to_text_fragment =
      GetScrollPositionAsTextFragment(&entry);
  if (scroll_to_text_fragment) {
    params.internal_scroll_to_text_fragment = *scroll_to_text_fragment;
  }

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);

  if (params.navigated_or_inserted_contents) {
    SendTabToSelfScrollObserver::CreateForWebContents(
        params.navigated_or_inserted_contents,
        /*restoration_attempted=*/scroll_to_text_fragment.has_value());
  }

  if (handle &&
      base::FeatureList::IsEnabled(kSendTabToSelfPropagateFormFields)) {
    FillWebContents(params.navigated_or_inserted_contents,
                    url::Origin::Create(entry.GetURL()),
                    entry.GetPageContext());
  }

  SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->GetSendTabToSelfModel()
      ->MarkEntryOpened(entry.GetGUID());

  return params.navigated_or_inserted_contents
             ? params.navigated_or_inserted_contents->GetWeakPtr()
             : nullptr;
}

}  // namespace

base::WeakPtr<content::WebContents> OpenEntryInNewForegroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry) {
  return OpenEntryInNewTabWithDisposition(
      profile, entry, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

base::WeakPtr<content::WebContents> OpenEntryInNewBackgroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry) {
  return OpenEntryInNewTabWithDisposition(
      profile, entry, WindowOpenDisposition::NEW_BACKGROUND_TAB);
}

void ShowTabSentSuccessToast(content::WebContents* web_contents,
                             std::string_view device_name) {
  ToastController* toast_controller =
      ToastController::MaybeGetForWebContents(web_contents);
  if (toast_controller) {
    ToastParams params(ToastId::kSendTabToSelfSuccess);
    // TODO(mtatarski): The string "Sent to Chrome on your [device]" only really
    // makes sense if `device_name` is a device type/class (e.g. "iPhone" or "Pixel"),
    // not a custom name or hostname. Check with UX if we should use manufacturer
    // + form factor to construct the device name where available, and how to
    // handle edge cases like custom-made PCs.
    params.body_string_replacement_params = {base::UTF8ToUTF16(device_name)};
    toast_controller->MaybeShowToast(std::move(params));
  }
}

void ShowTabSentThrottledToast(content::WebContents* web_contents,
                               std::string_view device_name) {
  ToastController* toast_controller =
      ToastController::MaybeGetForWebContents(web_contents);
  if (toast_controller) {
    ToastParams params(ToastId::kSendTabToSelfSuccessThrottled);
    params.body_string_replacement_params = {base::UTF8ToUTF16(device_name)};
    toast_controller->MaybeShowToast(std::move(params));
  }
}

void ShowTabSentFailure(content::WebContents* web_contents, const GURL& url) {
  CHECK(web_contents);
  // If the post-send toast feature is enabled, shows a modern Toast UI.
  // TODO(crbug.com/492072882): The generic failure string is temporary and
  // shall be replaced once all final strings are specified.
  if (base::FeatureList::IsEnabled(kSendTabToSelfPostSendToast)) {
    ToastController* toast_controller =
        ToastController::MaybeGetForWebContents(web_contents);
    if (toast_controller) {
      toast_controller->MaybeShowToast(
          ToastParams(ToastId::kSendTabToSelfFailure));
    }
  } else {
    // Fallback to legacy system notification if the toast feature is disabled.
    GURL notification_url =
        url.is_empty() ? web_contents->GetLastCommittedURL() : url;
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());

    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        "shared" + base::Uuid::GenerateRandomV4().AsLowercaseString(),
        l10n_util::GetStringUTF16(
            IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_CONFIRMATION_FAILURE_TITLE),
        l10n_util::GetStringUTF16(
            IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_CONFIRMATION_FAILURE_MESSAGE),
        ui::ImageModel(), base::UTF8ToUTF16(notification_url.host()),
        notification_url, message_center::NotifierId(notification_url),
        message_center::RichNotificationData(),
        /*delegate=*/nullptr);

    NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
        NotificationHandler::Type::SHARING, notification,
        /*metadata=*/nullptr);
  }
}

}  // namespace send_tab_to_self
