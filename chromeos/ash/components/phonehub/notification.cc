// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/notification.h"

#include <tuple>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash::phonehub {

const char kVisibleAppName[] = "visible_app_name";
const char kPackageName[] = "package_name";
const char kUserId[] = "user_id";
const char kAppStreamabilityStatus[] = "app_streamability_status";
const char kColorIcon[] = "icon";
const char kIconColorR[] = "icon_color_r";
const char kIconColorG[] = "icon_color_g";
const char kIconColorB[] = "icon_color_b";
const char kIconIsMonochrome[] = "icon_is_monochrome";
const char kMonochromeIconMask[] = "monochrome_icon_mask";

Notification::AppMetadata::AppMetadata(
    const std::u16string& visible_app_name,
    const std::string& package_name,
    const gfx::Image& color_icon,
    const std::optional<gfx::Image>& monochrome_icon_mask,
    const std::optional<SkColor> icon_color,
    bool icon_is_monochrome,
    int64_t user_id,
    proto::AppStreamabilityStatus app_streamability_status)
    : visible_app_name(visible_app_name),
      package_name(package_name),
      color_icon(color_icon),
      monochrome_icon_mask(monochrome_icon_mask),
      icon_color(icon_color),
      icon_is_monochrome(icon_is_monochrome),
      user_id(user_id),
      app_streamability_status(app_streamability_status) {}

Notification::AppMetadata::~AppMetadata() = default;

Notification::AppMetadata::AppMetadata(const AppMetadata& other) = default;

Notification::AppMetadata& Notification::AppMetadata::operator=(
    const AppMetadata& other) = default;

bool Notification::AppMetadata::operator==(const AppMetadata& other) const {
  return visible_app_name == other.visible_app_name &&
         package_name == other.package_name && color_icon == other.color_icon &&
         user_id == other.user_id;
}

bool Notification::AppMetadata::operator!=(const AppMetadata& other) const {
  return !(*this == other);
}

base::Value::Dict Notification::AppMetadata::ToValue() const {
  scoped_refptr<base::RefCountedMemory> png_data = color_icon.As1xPNGBytes();
  scoped_refptr<base::RefCountedMemory> monochrome_mask_png_data;

  base::Value::Dict val;
  val.Set(kVisibleAppName, visible_app_name);
  val.Set(kPackageName, package_name);
  val.Set(kUserId, static_cast<double>(user_id));
  val.Set(kColorIcon, base::Base64Encode(*png_data));

  if (monochrome_icon_mask.has_value()) {
    monochrome_mask_png_data = monochrome_icon_mask.value().As1xPNGBytes();
    val.Set(kMonochromeIconMask, base::Base64Encode(*monochrome_mask_png_data));
  }

  val.Set(kIconIsMonochrome, icon_is_monochrome);
  if (icon_color.has_value()) {
    val.Set(kIconColorR, static_cast<int>(SkColorGetR(*icon_color)));
    val.Set(kIconColorG, static_cast<int>(SkColorGetG(*icon_color)));
    val.Set(kIconColorB, static_cast<int>(SkColorGetB(*icon_color)));
  }
  val.Set(kAppStreamabilityStatus, static_cast<int>(app_streamability_status));
  return val;
}

// static
Notification::AppMetadata Notification::AppMetadata::FromValue(
    const base::Value::Dict& value) {
  DCHECK(value.FindString(kVisibleAppName));
  DCHECK(value.FindString(kPackageName));
  DCHECK(value.FindDouble(kUserId));
  DCHECK(value.FindString(kColorIcon));

  if (value.contains(kMonochromeIconMask)) {
    DCHECK(value.FindString(kMonochromeIconMask));
  }

  if (value.contains(kIconIsMonochrome)) {
    DCHECK(value.FindBool(kIconIsMonochrome));
  }
  bool icon_is_monochrome =
      value.FindBoolByDottedPath(kIconIsMonochrome).value_or(false);

  std::optional<SkColor> icon_color = std::nullopt;
  if (value.contains(kIconColorR)) {
    DCHECK(value.FindInt(kIconColorR));
    DCHECK(value.FindInt(kIconColorG));
    DCHECK(value.FindInt(kIconColorB));
    icon_color = SkColorSetRGB(*(value.FindInt(kIconColorR)),
                               *(value.FindInt(kIconColorG)),
                               *(value.FindInt(kIconColorB)));
  }

  const base::Value* visible_app_name_value = value.Find(kVisibleAppName);
  std::u16string visible_app_name_string_value;
  if (visible_app_name_value->is_string()) {
    visible_app_name_string_value =
        base::UTF8ToUTF16(visible_app_name_value->GetString());
  }

  std::string color_icon_str;
  base::Base64Decode(*(value.FindString(kColorIcon)), &color_icon_str);
  gfx::Image decode_color_icon = gfx::Image::CreateFrom1xPNGBytes(
      base::MakeRefCounted<base::RefCountedString>(std::move(color_icon_str)));

  std::optional<gfx::Image> decode_monochrome_icon_mask = std::nullopt;
  std::string monochrome_icon_mask_str;
  if (value.contains(kMonochromeIconMask)) {
    base::Base64Decode(*(value.FindString(kMonochromeIconMask)),
                       &monochrome_icon_mask_str);
    decode_monochrome_icon_mask = gfx::Image::CreateFrom1xPNGBytes(
        base::MakeRefCounted<base::RefCountedString>(
            std::move(monochrome_icon_mask_str)));
  }

  return Notification::AppMetadata(
      visible_app_name_string_value, *(value.FindString(kPackageName)),
      decode_color_icon, decode_monochrome_icon_mask, icon_color,
      icon_is_monochrome, *(value.FindDouble(kUserId)),
      static_cast<proto::AppStreamabilityStatus>(
          value.FindInt(kAppStreamabilityStatus)
              .value_or(static_cast<int>(
                  proto::AppStreamabilityStatus::STREAMABLE))));
}

Notification::Notification(
    int64_t id,
    const AppMetadata& app_metadata,
    const base::Time& timestamp,
    Importance importance,
    Notification::Category category,
    const base::flat_map<Notification::ActionType, int64_t>& action_id_map,
    InteractionBehavior interaction_behavior,
    const std::optional<std::u16string>& title,
    const std::optional<std::u16string>& text_content,
    const std::optional<gfx::Image>& shared_image,
    const std::optional<gfx::Image>& contact_image)
    : id_(id),
      app_metadata_(app_metadata),
      timestamp_(timestamp),
      importance_(importance),
      category_(category),
      action_id_map_(action_id_map),
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
         category_ == other.category_ &&
         action_id_map_ == other.action_id_map_ &&
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
                         Notification::Category category) {
  switch (category) {
    case Notification::Category::kNone:
      stream << "[None]";
      break;
    case Notification::Category::kConversation:
      stream << "[Conversation]";
      break;
    case Notification::Category::kIncomingCall:
      stream << "[IncomingCall]";
      break;
    case Notification::Category::kOngoingCall:
      stream << "[OngoingCall]";
      break;
    case Notification::Category::kScreenCall:
      stream << "[ScreenCall]";
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
         << "Category: " << notification.category() << ", "
         << "InteractionBehavior: " << notification.interaction_behavior()
         << "}";
  return stream;
}

}  // namespace ash::phonehub
