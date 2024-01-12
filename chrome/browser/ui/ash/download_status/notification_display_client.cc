// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/notification_display_client.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace ash::download_status {

namespace {

// Constants -------------------------------------------------------------------

constexpr char kNotificationNotifierId[] =
    "chrome://downloads/notification/id-notifier";

constexpr char kNotificationOrigin[] = "chrome://downloads";

// The commands supported by notification buttons.
constexpr std::array<CommandType, 3> kButtonCommands = {
    CommandType::kCancel, CommandType::kPause, CommandType::kResume};

// DownloadNotificationDelegate ------------------------------------------------

class DownloadNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  DownloadNotificationDelegate(
      std::vector<base::RepeatingClosure> button_click_callbacks,
      base::RepeatingClosure body_click_callback,
      base::RepeatingClosure closed_by_user_callback)
      : button_click_callbacks_(std::move(button_click_callbacks)),
        body_click_callback_(std::move(body_click_callback)),
        closed_by_user_callback_(std::move(closed_by_user_callback)) {}
  DownloadNotificationDelegate(const DownloadNotificationDelegate&) = delete;
  DownloadNotificationDelegate& operator=(const DownloadNotificationDelegate&) =
      delete;

 private:
  // message_center::NotificationDelegate:
  ~DownloadNotificationDelegate() override = default;

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    if (button_index >= 0 && button_index < button_click_callbacks_.size()) {
      button_click_callbacks_[*button_index].Run();
      return;
    }

    if (!button_index) {
      body_click_callback_.Run();
    }
  }

  void Close(bool by_user) override {
    if (by_user) {
      closed_by_user_callback_.Run();
    }
  }

  // Callbacks for handling button click events, listed in the order of their
  // corresponding buttons.
  const std::vector<base::RepeatingClosure> button_click_callbacks_;

  // Runs when the notification body is clicked.
  const base::RepeatingClosure body_click_callback_;

  // Runs when the observed notification is closed by user.
  const base::RepeatingClosure closed_by_user_callback_;
};

// Helpers ---------------------------------------------------------------------

// Returns the callback that runs when the notification body associated with
// `display_metadata` is clicked. Performs the show-in-browser command if
// `display_metadata` contains it. Otherwise, the download file will be opened.
base::RepeatingClosure GetNotificationBodyClickCallback(
    Profile* profile,
    const DisplayMetadata& display_metadata) {
  const std::vector<CommandInfo>& command_infos =
      display_metadata.command_infos;
  if (auto show_in_browser_iter = base::ranges::find(
          command_infos, CommandType::kShowInBrowser, &CommandInfo::type);
      show_in_browser_iter != command_infos.end()) {
    return show_in_browser_iter->command_callback;
  }

  // TODO(http://b/316368295): Track successful file openings as a metric.
  return base::BindRepeating(
      [](const AccountId& account_id, const base::FilePath& file_path) {
        if (Profile* const profile =
                ProfileHelper::Get()->GetProfileByAccountId(account_id)) {
          platform_util::OpenItem(profile, file_path,
                                  platform_util::OpenItemType::OPEN_FILE,
                                  /*callback=*/base::DoNothing());
        }
      },
      ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId(),
      display_metadata.file_path);
}

// NOTE: This function returns a non-empty string indicating the notification
// text, but does not guarantee the presence of a notification.
std::string GetNotificationIdFromGuid(const std::string& guid) {
  return base::StrCat({kNotificationNotifierId, "/", guid});
}

}  // namespace

NotificationDisplayClient::NotificationDisplayClient(Profile* profile)
    : DisplayClient(profile) {
  CHECK(features::IsSysUiDownloadsIntegrationV2Enabled());
}

NotificationDisplayClient::~NotificationDisplayClient() = default;

void NotificationDisplayClient::AddOrUpdate(
    const std::string& guid,
    const DisplayMetadata& display_metadata) {
  // Do not show the notification if it has been closed by user.
  if (base::Contains(notifications_closed_by_user_guids_, guid)) {
    return;
  }

  // Get button infos from `display_metadata`.
  std::vector<base::RepeatingClosure> button_click_callbacks;
  std::vector<message_center::ButtonInfo> buttons;
  for (const auto& command_info : display_metadata.command_infos) {
    if (base::Contains(kButtonCommands, command_info.type)) {
      button_click_callbacks.push_back(command_info.command_callback);
      buttons.emplace_back(l10n_util::GetStringUTF16(command_info.text_id));
    }
  }

  // Calculate progress from `display_metadata`.
  int progress = 0;
  const std::optional<int64_t>& received_bytes =
      display_metadata.received_bytes;
  const std::optional<int64_t>& total_bytes = display_metadata.total_bytes;
  if (received_bytes >= 0 && total_bytes > 0) {
    progress = *received_bytes * 100.f / *total_bytes;
  } else {
    // A negative progress value shows an indeterminate progress bar.
    progress = -1;
  }

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons = std::move(buttons);
  rich_notification_data.fullscreen_visibility =
      message_center::FullscreenVisibility::OVER_USER;
  rich_notification_data.progress = progress;
  rich_notification_data.progress_status =
      display_metadata.secondary_text.value_or(std::u16string());
  rich_notification_data.should_make_spoken_feedback_for_popup_updates = false;
  rich_notification_data.vector_small_image = &kNotificationDownloadIcon;

  message_center::Notification notification =
      SystemNotificationBuilder()
          .SetDelegate(base::MakeRefCounted<DownloadNotificationDelegate>(
              std::move(button_click_callbacks),
              GetNotificationBodyClickCallback(profile(), display_metadata),
              base::BindRepeating(
                  &NotificationDisplayClient::OnNotificationClosedByUser,
                  weak_ptr_factory_.GetWeakPtr(), guid)))
          .SetDisplaySource(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_NOTIFICATION_DISPLAY_SOURCE))
          .SetId(GetNotificationIdFromGuid(guid))
          .SetNotifierId(message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotificationNotifierId,
              NotificationCatalogName::kDownloadNotification))
          .SetOptionalFields(std::move(rich_notification_data))
          .SetOriginUrl(GURL(kNotificationOrigin))
          .SetTitle(display_metadata.text.value_or(std::u16string()))
          .SetType(message_center::NOTIFICATION_TYPE_PROGRESS)
          .Build(/*keep_timestamp=*/false);

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, std::move(notification),
      /*metadata=*/nullptr);

  // TODO(http://b/306459683): Change this code after `DisplayMetadata` uses a
  // data structure to represent download progress.
  if (received_bytes > 0 && received_bytes == total_bytes) {
    // The download associated with `guid` completes. We no longer anticipate
    // receiving download updates. Therefore, remove `guid` from the collection.
    notifications_closed_by_user_guids_.erase(guid);
  }
}

void NotificationDisplayClient::Remove(const std::string& guid) {
  // The download associated with `guid` is removed. We no longer anticipate
  // receiving download updates. Therefore, remove `guid` from the collection.
  notifications_closed_by_user_guids_.erase(guid);

  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationIdFromGuid(guid));
}

void NotificationDisplayClient::OnNotificationClosedByUser(
    const std::string& guid) {
  notifications_closed_by_user_guids_.insert(guid);
}

}  // namespace ash::download_status
