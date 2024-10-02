// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/notification_display_client.h"

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
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/vector_icons/vector_icons.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace ash::download_status {

namespace {

// Constants -------------------------------------------------------------------

// A notification image's preferred size.
constexpr gfx::Size kNotificationImagePreferredSize(/*width=*/360,
                                                    /*height=*/240);

constexpr char kNotificationNotifierId[] =
    "chrome://downloads/notification/id-notifier";

constexpr char kNotificationOrigin[] = "chrome://downloads";

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

// Calculates the progress value accepted by the notification progress bar.
// Returns a `std::nullopt` if the notification progress bar should be hidden.
std::optional<int> CalculateProgressValue(const Progress& progress) {
  // When `progress` is hidden, the notification's progress bar does not show.
  if (progress.hidden()) {
    return std::nullopt;
  }

  const std::optional<int64_t>& received_bytes = progress.received_bytes();
  const std::optional<int64_t>& total_bytes = progress.total_bytes();

  // NOTE: `total_bytes` could be zero. Therefore, check the equality of
  // `received_bytes` and `total_bytes` before division. In addition, the
  // equality of `received_bytes` and `total_bytes` does not necessarily mean
  // that `complete` is true.
  if (progress.complete() ||
      (received_bytes && received_bytes == total_bytes)) {
    return 100;
  }

  if (received_bytes >= 0 && total_bytes > 0) {
    return *received_bytes * 100.f / *total_bytes;
  }

  // Indicate an indeterminate progress bar.
  return -1;
}

const char* GetMetricString(CommandType command) {
  switch (command) {
    case CommandType::kCancel:
      return "DownloadNotificationV2.Button_Cancel";
    case CommandType::kCopyToClipboard:
      return "DownloadNotificationV2.Button_CopyToClipboard";
    case CommandType::kEditWithMediaApp:
      return "DownloadNotificationV2.Button_EditWithMediaApp";
    case CommandType::kOpenFile:
      return "DownloadNotificationV2.Click_Completed";
    case CommandType::kOpenWithMediaApp:
      return "DownloadNotificationV2.Button_OpenWithMediaApp";
    case CommandType::kPause:
      return "DownloadNotificationV2.Button_Pause";
    case CommandType::kResume:
      return "DownloadNotificationV2.Button_Resume";
    case CommandType::kShowInBrowser:
      return "DownloadNotificationV2.Click_InProgress";
    case CommandType::kShowInFolder:
      return "DownloadNotificationV2.Button_ShowInFolder";
    case CommandType::kViewDetailsInBrowser:
      return "DownloadNotificationV2.Button_ViewDetailsInBrowser";
  }
}

// Returns true if the execution of `command` is triggered by a click on a
// notification body.
bool IsBodyClickCommandType(CommandType command) {
  switch (command) {
    case CommandType::kOpenFile:
    case CommandType::kShowInBrowser:
      return true;
    case CommandType::kCancel:
    case CommandType::kCopyToClipboard:
    case CommandType::kEditWithMediaApp:
    case CommandType::kOpenWithMediaApp:
    case CommandType::kPause:
    case CommandType::kResume:
    case CommandType::kShowInFolder:
    case CommandType::kViewDetailsInBrowser:
      return false;
  }
}

// Returns true if the execution of `command` is triggered by a click on a
// notification button.
bool IsButtonClickCommandType(CommandType command) {
  switch (command) {
    case CommandType::kCancel:
    case CommandType::kCopyToClipboard:
    case CommandType::kEditWithMediaApp:
    case CommandType::kOpenWithMediaApp:
    case CommandType::kPause:
    case CommandType::kResume:
    case CommandType::kShowInFolder:
    case CommandType::kViewDetailsInBrowser:
      return true;
    case CommandType::kOpenFile:
    case CommandType::kShowInBrowser:
      return false;
  }
}

void RecordCommand(CommandType command) {
  base::RecordAction(base::UserMetricsAction(GetMetricString(command)));
}

// Returns the callback that runs when the notification body associated with
// `display_metadata` is clicked.
base::RepeatingClosure GetNotificationBodyClickCallback(
    Profile* profile,
    const DisplayMetadata& display_metadata) {
  for (const auto& command : display_metadata.command_infos) {
    if (const CommandType type = command.type; IsBodyClickCommandType(type)) {
      return command.command_callback.Then(
          base::BindRepeating(&RecordCommand, type));
    }
  }

  LOG(ERROR) << "Failed to find a notification body click callback";
  return base::DoNothing();
}

// NOTE: This function returns a non-empty string indicating the notification
// text, but does not guarantee the presence of a notification.
std::string GetNotificationIdFromGuid(const std::string& guid) {
  return base::StrCat({kNotificationNotifierId, "/", guid});
}

// Returns a notification image from `original_image`. This function should be
// called only when the image of `original_image` is not null nor empty.
// NOTE: This function avoids using image skia operations to prevent unnecessary
// retention of original image data.
gfx::Image GetNotificationImage(const gfx::ImageSkia& original_image) {
  CHECK(!original_image.isNull());
  CHECK(!original_image.size().IsEmpty());

  const float target_aspect_ratio =
      static_cast<float>(kNotificationImagePreferredSize.width()) /
      kNotificationImagePreferredSize.height();
  const float original_aspect_ratio =
      static_cast<float>(original_image.width()) / original_image.height();

  // Get the largest rect from `original_image` that has `target_aspect_ratio`.
  gfx::Rect source_rect;
  if (original_aspect_ratio > target_aspect_ratio) {
    const float width = original_image.height() * target_aspect_ratio;
    source_rect = gfx::Rect(/*x=*/(original_image.width() - width) / 2,
                            /*y=*/0, width, original_image.height());
  } else {
    const float height = original_image.width() / target_aspect_ratio;
    source_rect =
        gfx::Rect(/*x=*/0, /*y=*/(original_image.height() - height) / 2,
                  original_image.width(), height);
  }
  const SkBitmap cropped_bitmap = SkBitmapOperations::CreateTiledBitmap(
      *original_image.bitmap(), source_rect.x(), source_rect.y(),
      source_rect.width(), source_rect.height());

  // Find the largest supported scale factor for the returned image without
  // upscaling `original_image`.
  gfx::Size scaled_preferred_size = kNotificationImagePreferredSize;
  float largest_scale = 1.f;
  for (const auto& scale_factor : ui::GetSupportedResourceScaleFactors()) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    if (scale <= 1.f) {
      continue;
    }

    if (const gfx::Size scaled_size =
            gfx::ScaleToCeiledSize(kNotificationImagePreferredSize, scale);
        gfx::Rect(original_image.size()).Contains(gfx::Rect(scaled_size))) {
      largest_scale = scale;
      scaled_preferred_size = scaled_size;
    }
  }

  const SkBitmap resized_bitmap = skia::ImageOperations::Resize(
      cropped_bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
      scaled_preferred_size.width(), scaled_preferred_size.height());

  return gfx::Image(
      gfx::ImageSkia::CreateFromBitmap(resized_bitmap, largest_scale));
}

}  // namespace

NotificationDisplayClient::NotificationDisplayClient(Profile* profile)
    : DisplayClient(profile) {}

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
    if (const CommandType type = command_info.type;
        IsButtonClickCommandType(type)) {
      button_click_callbacks.push_back(command_info.command_callback.Then(
          base::BindRepeating(&RecordCommand, type)));
      buttons.emplace_back(l10n_util::GetStringUTF16(command_info.text_id));
    }
  }

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons = std::move(buttons);
  rich_notification_data.fullscreen_visibility =
      message_center::FullscreenVisibility::OVER_USER;
  rich_notification_data.should_make_spoken_feedback_for_popup_updates = false;
  rich_notification_data.vector_small_image =
      &vector_icons::kNotificationDownloadIcon;

  const Progress& progress = display_metadata.progress;
  if (const std::optional<int> progress_value =
          CalculateProgressValue(progress)) {
    rich_notification_data.progress = *progress_value;
    rich_notification_data.progress_status =
        display_metadata.secondary_text.value_or(std::u16string());
  }

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
          .SetMessage(
              progress.hidden()
                  ? display_metadata.secondary_text.value_or(std::u16string())
                  : std::u16string())
          .SetNotifierId(message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotificationNotifierId,
              NotificationCatalogName::kDownloadNotification))
          .SetOptionalFields(std::move(rich_notification_data))
          .SetOriginUrl(GURL(kNotificationOrigin))
          .SetTitle(display_metadata.text.value_or(std::u16string()))
          .SetType(progress.hidden()
                       ? message_center::NOTIFICATION_TYPE_SIMPLE
                       : message_center::NOTIFICATION_TYPE_PROGRESS)
          .Build(/*keep_timestamp=*/false);

  if (const gfx::ImageSkia& image = display_metadata.image;
      !image.isNull() && !image.size().IsEmpty()) {
    notification.SetImage(GetNotificationImage(image));
    notification.set_image_path(display_metadata.file_path);
  }

  NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, std::move(notification),
      /*metadata=*/nullptr);

  if (progress.complete()) {
    // The download associated with `guid` completes. We no longer anticipate
    // receiving download updates. Therefore, remove `guid` from the collection.
    notifications_closed_by_user_guids_.erase(guid);
  }
}

void NotificationDisplayClient::Remove(const std::string& guid) {
  // The download associated with `guid` is removed. We no longer anticipate
  // receiving download updates. Therefore, remove `guid` from the collection.
  notifications_closed_by_user_guids_.erase(guid);

  NotificationDisplayServiceFactory::GetForProfile(profile())->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationIdFromGuid(guid));
}

void NotificationDisplayClient::OnNotificationClosedByUser(
    const std::string& guid) {
  notifications_closed_by_user_guids_.insert(guid);
}

}  // namespace ash::download_status
