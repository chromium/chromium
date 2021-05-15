// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_processor.h"

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_manager.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace phonehub {
namespace {

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

absl::optional<int64_t> GetInlineReplyIdFromProto(
    const proto::Notification& proto) {
  auto actions_it = std::find_if(
      proto.actions().begin(), proto.actions().end(), [](const auto& action) {
        return action.type() == proto::Action_InputType::Action_InputType_TEXT;
      });

  if (actions_it == proto.actions().end())
    return absl::nullopt;

  return actions_it->id();
}

Notification CreateInlineReplyNotification(const proto::Notification& proto,
                                           const gfx::Image& icon,
                                           const gfx::Image& shared_image,
                                           const gfx::Image& contact_image) {
  absl::optional<int64_t> inline_reply_id = GetInlineReplyIdFromProto(proto);
  DCHECK(inline_reply_id.has_value());

  auto actions_it = std::find_if(
      proto.actions().begin(), proto.actions().end(), [](const auto& action) {
        return action.type() == proto::Action_InputType::Action_InputType_OPEN;
      });
  bool includes_open_action = actions_it != proto.actions().end();

  absl::optional<std::u16string> title = absl::nullopt;
  if (!proto.title().empty())
    title = base::UTF8ToUTF16(proto.title());

  absl::optional<std::u16string> text_content = absl::nullopt;
  if (!proto.text_content().empty())
    text_content = base::UTF8ToUTF16(proto.text_content());

  absl::optional<gfx::Image> opt_shared_image = absl::nullopt;
  if (!shared_image.IsEmpty())
    opt_shared_image = shared_image;

  absl::optional<gfx::Image> opt_contact_image = absl::nullopt;
  if (!contact_image.IsEmpty())
    opt_contact_image = contact_image;

  return Notification(
      proto.id(),
      Notification::AppMetadata(
          base::UTF8ToUTF16(proto.origin_app().visible_name()),
          proto.origin_app().package_name(), icon),
      base::Time::FromJsTime(proto.epoch_time_millis()),
      GetNotificationImportanceFromProto(proto.importance()), *inline_reply_id,
      includes_open_action ? Notification::InteractionBehavior::kOpenable
                           : Notification::InteractionBehavior::kNone,
      title, text_content, opt_shared_image, opt_contact_image);
}

}  // namespace

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

  std::vector<proto::Notification> inline_replyable_notification_protos;
  std::vector<DecodeImageRequestMetadata> decode_image_requests;

  for (const auto& proto : notification_protos) {
    // Only process notifications that are messaging apps with inline-replies.
    if (!GetInlineReplyIdFromProto(proto).has_value())
      continue;

    inline_replyable_notification_protos.emplace_back(proto);

    decode_image_requests.emplace_back(
        proto.id(), NotificationImageField::kIcon, proto.origin_app().icon());

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

  base::RepeatingClosure barrier = base::BarrierClosure(
      decode_image_requests.size(),
      base::BindOnce(&NotificationProcessor::OnAllImagesDecoded,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(inline_replyable_notification_protos)));

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
  std::vector<uint8_t> image_bytes(data.begin(), data.end());
  data_decoder::DecodeImage(
      &data_decoder_, image_bytes, data_decoder::mojom::ImageCodec::kDefault,
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
    case NotificationImageField::kIcon:
      it->second.icon = gfx::Image(image_skia);
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
    std::vector<proto::Notification> inline_replyable_notifications) {
  base::flat_set<Notification> notifications;
  for (const auto& proto : inline_replyable_notifications) {
    auto it = id_to_images_map_.find(proto.id());
    if (it == id_to_images_map_.end())
      continue;

    NotificationImages notification_images = it->second;
    notifications.emplace(CreateInlineReplyNotification(
        proto, notification_images.icon, notification_images.shared_image,
        notification_images.contact_image));
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
}  // namespace chromeos
