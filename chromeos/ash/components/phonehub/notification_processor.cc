// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/notification_processor.h"

#include "ash/constants/ash_features.h"
#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/notification_manager.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace phonehub {

namespace {

// Constants to override the Messages app monochrome icon color.
const char kMessagesPackageName[] = "com.google.android.apps.messaging";
const SkColor kMessagesOverrideColor = gfx::kGoogleBlue600;

std::optional<SkColor> getMonochromeIconColor(const proto::Notification& proto,
                                              const gfx::Image& icon) {
  if (icon.IsEmpty() || !proto.origin_app().has_monochrome_icon_color()) {
    return std::nullopt;
  }

  if (proto.origin_app().package_name() == kMessagesPackageName) {
    // The notification color supplied by the Messages app (Bugle) is based
    // on light/dark mode of the phone, not the Chromebook, with no way to
    // query for both at runtime. These constants are used to override with
    // a fixed color. See conversation at b/207089786 for more details.
    return kMessagesOverrideColor;
  }

  return SkColorSetRGB(proto.origin_app().monochrome_icon_color().red(),
                       proto.origin_app().monochrome_icon_color().green(),
                       proto.origin_app().monochrome_icon_color().blue());
}

Notification::Importance GetNotificationImportanceFromProto(
    proto::NotificationImportance importance) {
  switch (importance) {
    case proto::NotificationImportance::UNSPECIFIED:
      return Notification::Importance::kUnspecified;
    case proto::NotificationImportance::NONE:
      return Notification::Importance::kNone;
    case proto::NotificationImportance::MIN:
      return Notification::Importance::kMin;
    case proto::NotificationImportance::LOW:
      return Notification::Importance::kLow;
    case proto::NotificationImportance::DEFAULT:
      return Notification::Importance::kDefault;
    case proto::NotificationImportance::HIGH:
      return Notification::Importance::kHigh;
    default:
      return Notification::Importance::kUnspecified;
  }
}

bool HasSupportedActionIdInProto(const proto::Notification& proto) {
  for (const auto& action : proto.actions()) {
    if (action.type() == proto::Action_InputType::Action_InputType_TEXT ||
        action.call_action() ==
            proto::Action_CallAction::Action_CallAction_ANSWER ||
        action.call_action() ==
            proto::Action_CallAction::Action_CallAction_DECLINE ||
        action.call_action() ==
            proto::Action_CallAction::Action_CallAction_HANGUP)
      return true;
  }
  return false;
}

bool IsMonochromeIconEnabled(const proto::Notification& notification_proto) {
  return features::IsPhoneHubMonochromeNotificationIconsEnabled() &&
         notification_proto.origin_app().has_monochrome_icon_mask();
}

Notification CreateInternalNotification(const proto::Notification& proto,
                                        const gfx::Image& color_icon,
                                        const gfx::Image& monochrome_icon,
                                        const gfx::Image& shared_image,
                                        const gfx::Image& contact_image) {
  base::flat_map<Notification::ActionType, int64_t> action_id_map;
  Notification::InteractionBehavior behavior =
      Notification::InteractionBehavior::kNone;
  for (const auto& action : proto.actions()) {
    if (action.type() == proto::Action_InputType::Action_InputType_TEXT) {
      action_id_map[Notification::ActionType::kInlineReply] = action.id();
    } else if (action.type() ==
               proto::Action_InputType::Action_InputType_OPEN) {
      behavior = Notification::InteractionBehavior::kOpenable;
    } else if (action.call_action() ==
               proto::Action_CallAction::Action_CallAction_ANSWER) {
      action_id_map[Notification::ActionType::kAnswer] = action.id();
    } else if (action.call_action() ==
               proto::Action_CallAction::Action_CallAction_DECLINE) {
      action_id_map[Notification::ActionType::kDecline] = action.id();
    } else if (action.call_action() ==
               proto::Action_CallAction::Action_CallAction_HANGUP) {
      action_id_map[Notification::ActionType::kHangup] = action.id();
    }
  }

  Notification::Category category = Notification::Category::kNone;
  switch (proto.category()) {
    case proto::Notification::Category::Notification_Category_UNSPECIFIED:
      category = Notification::Category::kNone;
      break;
    case proto::Notification::Category::Notification_Category_CONVERSATION:
      category = Notification::Category::kConversation;
      break;
    case proto::Notification::Category::Notification_Category_INCOMING_CALL:
      category = Notification::Category::kIncomingCall;
      break;
    case proto::Notification::Category::Notification_Category_ONGOING_CALL:
      category = Notification::Category::kOngoingCall;
      break;
    case proto::Notification::Category::Notification_Category_SCREEN_CALL:
      category = Notification::Category::kScreenCall;
      break;
    case proto::
        Notification_Category_Notification_Category_INT_MIN_SENTINEL_DO_NOT_USE_:  // NOLINT
    case proto::
        Notification_Category_Notification_Category_INT_MAX_SENTINEL_DO_NOT_USE_:  // NOLINT
      PA_LOG(WARNING) << "Notification category is unknown or unspecified.";
      break;
  }

  std::optional<std::u16string> title = std::nullopt;
  if (!proto.title().empty())
    title = base::UTF8ToUTF16(proto.title());

  std::optional<std::u16string> text_content = std::nullopt;
  if (!proto.text_content().empty())
    text_content = base::UTF8ToUTF16(proto.text_content());

  std::optional<gfx::Image> opt_shared_image = std::nullopt;
  if (!shared_image.IsEmpty())
    opt_shared_image = shared_image;

  std::optional<gfx::Image> opt_contact_image = std::nullopt;
  if (!contact_image.IsEmpty())
    opt_contact_image = contact_image;

  bool icon_is_monochrome = IsMonochromeIconEnabled(proto);
  std::optional<SkColor> icon_color =
      icon_is_monochrome ? getMonochromeIconColor(proto, monochrome_icon)
                         : std::nullopt;

  return Notification(
      proto.id(),
      Notification::AppMetadata(
          base::UTF8ToUTF16(proto.origin_app().visible_name()),
          proto.origin_app().package_name(), color_icon, monochrome_icon,
          icon_color, icon_is_monochrome, proto.origin_app().user_id(),
          proto.origin_app().app_streamability_status()),
      base::Time::FromMillisecondsSinceUnixEpoch(proto.epoch_time_millis()),
      GetNotificationImportanceFromProto(proto.importance()), category,
      action_id_map, behavior, title, text_content, opt_shared_image,
      opt_contact_image);
}

}  // namespace

NotificationProcessor::NotificationImages::NotificationImages() = default;

NotificationProcessor::NotificationImages::~NotificationImages() = default;

NotificationProcessor::NotificationImages::NotificationImages(
    const NotificationImages& other) = default;

NotificationProcessor::NotificationImages&
NotificationProcessor::NotificationImages::operator=(
    const NotificationImages& other) = default;

NotificationProcessor::DecodeImageRequestMetadata::DecodeImageRequestMetadata(
    int64_t notification_id,
    NotificationImageField image_field,
    const std::string& data)
    : notification_id(notification_id), image_field(image_field), data(data) {}

NotificationProcessor::NotificationProcessor(
    NotificationManager* notification_manager)
    : NotificationProcessor(notification_manager,
                            std::make_unique<ImageDecoderDelegate>()) {}

NotificationProcessor::NotificationProcessor(
    NotificationManager* notification_manager,
    std::unique_ptr<ImageDecoderDelegate> delegate)
    : notification_manager_(notification_manager),
      delegate_(std::move(delegate)) {}

NotificationProcessor::~NotificationProcessor() {}

void NotificationProcessor::ClearNotificationsAndPendingUpdates() {
  notification_manager_->ClearNotificationsInternal();

  // Clear pending updates that may occur.
  weak_ptr_factory_.InvalidateWeakPtrs();
  pending_notification_requests_ = base::queue<base::OnceClosure>();
  id_to_images_map_.clear();
}

void NotificationProcessor::AddNotifications(
    const std::vector<proto::Notification>& notification_protos) {
  if (notification_protos.empty())
    return;

  std::vector<proto::Notification> processed_notification_protos;
  std::vector<DecodeImageRequestMetadata> decode_image_requests;

  for (const auto& proto : notification_protos) {
    // Only process notifications that are messaging apps with inline-replies
    // or dialer apps with call-style actions.
    if (!HasSupportedActionIdInProto(proto))
      continue;

    processed_notification_protos.emplace_back(proto);

    decode_image_requests.emplace_back(proto.id(),
                                       NotificationImageField::kColorIcon,
                                       proto.origin_app().icon());

    if (IsMonochromeIconEnabled(proto)) {
      decode_image_requests.emplace_back(
          proto.id(), NotificationImageField::kMonochromeIcon,
          proto.origin_app().monochrome_icon_mask());
    } else {
      decode_image_requests.emplace_back(
          proto.id(), NotificationImageField::kMonochromeIcon,
          proto.origin_app().icon());
    }

    if (!proto.shared_image().empty()) {
      decode_image_requests.emplace_back(proto.id(),
                                         NotificationImageField::kSharedImage,
                                         proto.shared_image());
    }

    if (!proto.contact_image().empty()) {
      decode_image_requests.emplace_back(proto.id(),
                                         NotificationImageField::kContactImage,
                                         proto.contact_image());
    }
  }

  if (decode_image_requests.empty()) {
    PA_LOG(INFO) << "Cannot find any image to decode for the notifications";
    return;
  }

  base::RepeatingClosure barrier = base::BarrierClosure(
      decode_image_requests.size(),
      base::BindOnce(&NotificationProcessor::OnAllImagesDecoded,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(processed_notification_protos)));

  base::OnceClosure add_request =
      base::BindOnce(&NotificationProcessor::StartDecodingImages,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(decode_image_requests), barrier);

  pending_notification_requests_.emplace(std::move(add_request));
  ProcessRequestQueue();
}

void NotificationProcessor::RemoveNotifications(
    const base::flat_set<int64_t>& notification_ids) {
  if (notification_ids.empty())
    return;

  base::flat_set<int64_t> removed_notification_ids;
  for (const int64_t& id : notification_ids) {
    removed_notification_ids.emplace(id);
  }

  base::OnceClosure remove_request = base::BindOnce(
      &NotificationProcessor::RemoveNotificationsAndProcessNextRequest,
      weak_ptr_factory_.GetWeakPtr(), std::move(removed_notification_ids));

  pending_notification_requests_.emplace(std::move(remove_request));
  ProcessRequestQueue();
}

void NotificationProcessor::StartDecodingImages(
    const std::vector<DecodeImageRequestMetadata>& decode_image_requests,
    base::RepeatingClosure done_closure) {
  DCHECK(!decode_image_requests.empty());
  DCHECK(!done_closure.is_null());

  id_to_images_map_.clear();

  for (const auto& request : decode_image_requests) {
    delegate_->PerformImageDecode(
        request.data,
        base::BindOnce(&NotificationProcessor::OnDecodedBitmapReady,
                       weak_ptr_factory_.GetWeakPtr(), request, done_closure));
  }
}

void NotificationProcessor::ImageDecoderDelegate::PerformImageDecode(
    const std::string& data,
    DecodeImageCallback single_image_decoded_closure) {
  data_decoder::DecodeImage(
      &data_decoder_, base::as_bytes(base::make_span(data)),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      std::move(single_image_decoded_closure));
}

void NotificationProcessor::OnDecodedBitmapReady(
    const DecodeImageRequestMetadata& request,
    base::OnceClosure done_closure,
    const SkBitmap& decoded_bitmap) {
  gfx::ImageSkia image_skia =
      gfx::ImageSkia::CreateFrom1xBitmap(decoded_bitmap);

  // If |image_skia| is null, indicating that the data decoder failed to decode
  // the image, the image will be empty, and cannot be made thread safe.
  if (!image_skia.isNull())
    image_skia.MakeThreadSafe();

  auto it = id_to_images_map_.find(request.notification_id);
  if (it == id_to_images_map_.end()) {
    it = id_to_images_map_.insert(
        it, std::pair<int64_t, NotificationImages>(request.notification_id,
                                                   NotificationImages()));
  }

  switch (request.image_field) {
    case NotificationImageField::kColorIcon:
      it->second.color_icon = gfx::Image(image_skia);
      break;
    case NotificationImageField::kMonochromeIcon:
      it->second.monochrome_icon_mask = gfx::Image(image_skia);
      break;
    case NotificationImageField::kSharedImage:
      it->second.shared_image = gfx::Image(image_skia);
      break;
    case NotificationImageField::kContactImage:
      it->second.contact_image = gfx::Image(image_skia);
      break;
  }

  std::move(done_closure).Run();
}

void NotificationProcessor::OnAllImagesDecoded(
    std::vector<proto::Notification> notification_protos) {
  base::flat_set<Notification> notifications;
  for (const auto& proto : notification_protos) {
    auto it = id_to_images_map_.find(proto.id());
    if (it == id_to_images_map_.end())
      continue;

    NotificationImages notification_images = it->second;
    notifications.emplace(CreateInternalNotification(
        proto, notification_images.color_icon,
        notification_images.monochrome_icon_mask,
        notification_images.shared_image, notification_images.contact_image));
  }

  AddNotificationsAndProcessNextRequest(notifications);
}

void NotificationProcessor::ProcessRequestQueue() {
  if (pending_notification_requests_.empty())
    return;

  // Processing the latest request has not been completed.
  if (pending_notification_requests_.front().is_null())
    return;

  std::move(pending_notification_requests_.front()).Run();
}

void NotificationProcessor::CompleteRequest() {
  DCHECK(!pending_notification_requests_.empty());
  DCHECK(pending_notification_requests_.front().is_null());
  pending_notification_requests_.pop();
  ProcessRequestQueue();
}

void NotificationProcessor::AddNotificationsAndProcessNextRequest(
    const base::flat_set<Notification>& notifications) {
  notification_manager_->SetNotificationsInternal(notifications);
  CompleteRequest();
}

void NotificationProcessor::RemoveNotificationsAndProcessNextRequest(
    base::flat_set<int64_t> removed_notification_ids) {
  notification_manager_->RemoveNotificationsInternal(removed_notification_ids);
  CompleteRequest();
}

}  // namespace phonehub
}  // namespace ash
