// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification.h"

#include <tuple>

#include "base/logging.h"

namespace chromeos {
namespace phonehub {

Notification::AppMetadata::AppMetadata(const std::u16string& visible_app_name,
                                       const std::string& package_name,
                                       const gfx::Image& icon)
    : visible_app_name(visible_app_name),
      package_name(package_name),
      icon(icon) {}

Notification::AppMetadata::AppMetadata(const AppMetadata& other) = default;

bool Notification::AppMetadata::operator==(const AppMetadata& other) const {
  return visible_app_name == other.visible_app_name &&
         package_name == other.package_name && icon == other.icon;
}

bool Notification::AppMetadata::operator!=(const AppMetadata& other) const {
  return !(*this == other);
}

Notification::Notification(int64_t id,
                           const AppMetadata& app_metadata,
                           const base::Time& timestamp,
                           Importance importance,
                           int64_t inline_reply_id,
                           InteractionBehavior interaction_behavior,
                           const base::Optional<std::u16string>& title,
                           const base::Optional<std::u16string>& text_content,
                           const base::Optional<gfx::Image>& shared_image,
                           const base::Optional<gfx::Image>& contact_image)
    : id_(id),
      app_metadata_(app_metadata),
      timestamp_(timestamp),
      importance_(importance),
      inline_reply_id_(inline_reply_id),
      interaction_behavior_(interaction_behavior),
      title_(title),
      text_content_(text_content),
      shared_image_(shared_image),
      contact_image_(contact_image) {}

Notification::Notification(const Notification& other) = default;

Notification::~Notification() = default;

bool Notification::operator<(const Notification& other) const {
  return std::tie(timestamp_, id_) < std::tie(other.timestamp_, other.id_);
}

bool Notification::operator==(const Notification& other) const {
  return id_ == other.id_ && app_metadata_ == other.app_metadata_ &&
         timestamp_ == other.timestamp_ && importance_ == other.importance_ &&
         inline_reply_id_ == other.inline_reply_id_ &&
         interaction_behavior_ == other.interaction_behavior_ &&
         title_ == other.title_ && text_content_ == other.text_content_ &&
         shared_image_ == other.shared_image_ &&
         contact_image_ == other.contact_image_;
}

bool Notification::operator!=(const Notification& other) const {
  return !(*this == other);
}

std::ostream& operator<<(std::ostream& stream,
                         const Notification::AppMetadata& app_metadata) {
  stream << "{VisibleAppName: \"" << app_metadata.visible_app_name << "\", "
         << "PackageName: \"" << app_metadata.package_name << "\"}";
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         Notification::Importance importance) {
  switch (importance) {
    case Notification::Importance::kUnspecified:
      stream << "[Unspecified]";
      break;
    case Notification::Importance::kNone:
      stream << "[None]";
      break;
    case Notification::Importance::kMin:
      stream << "[Min]";
      break;
    case Notification::Importance::kLow:
      stream << "[Low]";
      break;
    case Notification::Importance::kDefault:
      stream << "[Default]";
      break;
    case Notification::Importance::kHigh:
      stream << "[High]";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         Notification::InteractionBehavior behavior) {
  switch (behavior) {
    case Notification::InteractionBehavior::kNone:
      stream << "[None]";
      break;
    case Notification::InteractionBehavior::kOpenable:
      stream << "[Openable]";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const Notification& notification) {
  stream << "{Id: " << notification.id() << ", "
         << "App: " << notification.app_metadata() << ", "
         << "Timestamp: " << notification.timestamp() << ", "
         << "Importance: " << notification.importance() << ", "
         << "InteractionBehavior: " << notification.interaction_behavior()
         << "}";
  return stream;
}

}  // namespace phonehub
}  // namespace chromeos
