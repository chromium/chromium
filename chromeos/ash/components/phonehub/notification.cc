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

namespace ash {
namespace phonehub {

const char kVisibleAppName[] = "visible_app_name";
const char kPackageName[] = "package_name";
const char kUserId[] = "user_id";
const char kAppStreamabilityStatus[] = "app_streamability_status";
const char kIcon[] = "icon";
const char kIconColorR[] = "icon_color_r";
const char kIconColorG[] = "icon_color_g";
const char kIconColorB[] = "icon_color_b";
const char kIconIsMonochrome[] = "icon_is_monochrome";

Notification::AppMetadata::AppMetadata(
    const std::u16string& visible_app_name,
    const std::string& package_name,
    const gfx::Image& icon,
    const absl::optional<SkColor> icon_color,
    bool icon_is_monochrome,
    int64_t user_id,
    proto::AppStreamabilityStatus app_streamability_status)
    : visible_app_name(visible_app_name),
      package_name(package_name),
      icon(icon),
      icon_color(icon_color),
      icon_is_monochrome(icon_is_monochrome),
      user_id(user_id),
      app_streamability_status(app_streamability_status) {}

Notification::AppMetadata::AppMetadata(const AppMetadata& other) = default;

Notification::AppMetadata& Notification::AppMetadata::operator=(
    const AppMetadata& other) = default;

bool Notification::AppMetadata::operator==(const AppMetadata& other) const {
  return visible_app_name == other.visible_app_name &&
         package_name == other.package_name && icon == other.icon &&
         user_id == other.user_id;
}

bool Notification::AppMetadata::operator!=(const AppMetadata& other) const {
  return !(*this == other);
}

base::Value Notification::AppMetadata::ToValue() const {
  scoped_refptr<base::RefCountedMemory> png_data = icon.As1xPNGBytes();

  base::Value val(base::Value::Type::DICT);
  val.SetKey(kVisibleAppName, base::Value(visible_app_name));
  val.SetKey(kPackageName, base::Value(package_name));
  val.SetDoubleKey(kUserId, user_id);
  val.SetKey(kIcon, base::Value(base::Base64Encode(*png_data)));
  val.SetBoolKey(kIconIsMonochrome, icon_is_monochrome);
  if (icon_color.has_value()) {
    val.SetIntKey(kIconColorR, SkColorGetR(*icon_color));
    val.SetIntKey(kIconColorG, SkColorGetG(*icon_color));
    val.SetIntKey(kIconColorB, SkColorGetB(*icon_color));
  }
  val.SetIntKey(kAppStreamabilityStatus,
                static_cast<int>(app_streamability_status));
  return val;
}

// static
Notification::AppMetadata Notification::AppMetadata::FromValue(
    const base::Value& value) {
  DCHECK(value.is_dict());
  const base::Value::Dict& dict = value.GetDict();
  DCHECK(dict.contains(kVisibleAppName));
  DCHECK(dict.FindString(kVisibleAppName));
  DCHECK(dict.contains(kPackageName));
  DCHECK(dict.FindString(kPackageName));
  DCHECK(dict.contains(kUserId));
  DCHECK(dict.FindDouble(kUserId));
  DCHECK(dict.contains(kIcon));
  DCHECK(dict.FindString(kIcon));

  if (dict.contains(kIconIsMonochrome)) {
    DCHECK(dict.FindBool(kIconIsMonochrome));
  }
  bool icon_is_monochrome =
      dict.FindBoolByDottedPath(kIconIsMonochrome).value_or(false);

  absl::optional<SkColor> icon_color = absl::nullopt;
  if (dict.contains(kIconColorR)) {
    DCHECK(dict.FindInt(kIconColorR));
    DCHECK(dict.contains(kIconColorG));
    DCHECK(dict.FindInt(kIconColorG));
    DCHECK(dict.contains(kIconColorB));
    DCHECK(dict.FindInt(kIconColorB));
    icon_color = SkColorSetRGB(*(dict.FindIntByDottedPath(kIconColorR)),
                               *(dict.FindIntByDottedPath(kIconColorG)),
                               *(dict.FindIntByDottedPath(kIconColorB)));
  }

  const base::Value* visible_app_name_value = value.FindPath(kVisibleAppName);
  std::u16string visible_app_name_string_value;
  if (visible_app_name_value->is_string()) {
    visible_app_name_string_value =
        base::UTF8ToUTF16(visible_app_name_value->GetString());
  }

  std::string icon_str;
  base::Base64Decode(*(value.FindStringPath(kIcon)), &icon_str);
  gfx::Image decode_icon = gfx::Image::CreateFrom1xPNGBytes(
      base::MakeRefCounted<base::RefCountedString>(std::move(icon_str)));

  return Notification::AppMetadata(
      visible_app_name_string_value, *(value.FindStringPath(kPackageName)),
      decode_icon, icon_color, icon_is_monochrome,
      *(value.FindDoublePath(kUserId)),
      static_cast<proto::AppStreamabilityStatus>(
          value.FindIntPath(kAppStreamabilityStatus)
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
    const absl::optional<std::u16string>& title,
    const absl::optional<std::u16string>& text_content,
    const absl::optional<gfx::Image>& shared_image,
    const absl::optional<gfx::Image>& contact_image)
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
                         Notification::Category catetory) {
  switch (catetory) {
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

}  // namespace phonehub
}  // namespace ash
