// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_H_

#include <stdint.h>

#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ui/gfx/image/image.h"

// Serves the same purpose as a forward declare to avoid an extra include.
typedef uint32_t SkColor;

namespace ash::phonehub {

// A notification generated on the phone, whose contents are transferred to
// Chrome OS via a Phone Hub connection. Notifications in Phone Hub support
// inline reply and images.
class Notification {
 public:
  // Describes the app which generates a notification.
  struct AppMetadata {
    AppMetadata(const std::u16string& visible_app_name,
                const std::string& package_name,
                const gfx::Image& color_icon,
                const std::optional<gfx::Image>& monochrome_icon_mask,
                const std::optional<SkColor> icon_color,
                bool icon_is_monochrome,
                int64_t user_id,
                proto::AppStreamabilityStatus app_streamability_status =
                    proto::AppStreamabilityStatus::STREAMABLE);
    ~AppMetadata();
    AppMetadata(const AppMetadata& other);
    AppMetadata& operator=(const AppMetadata& other);

    bool operator==(const AppMetadata& other) const;
    bool operator!=(const AppMetadata& other) const;

    static AppMetadata FromValue(const base::Value::Dict& value);
    base::Value::Dict ToValue() const;

    std::u16string visible_app_name;
    std::string package_name;
    // The |color_icon| is the icon with it's original color whereas the
    // |monochrome_icon| is the icon with a monochrome or system theme mask.
    gfx::Image color_icon;
    std::optional<gfx::Image> monochrome_icon_mask;
    // Color for a monochrome icon. Leave empty to use the system theme default.
    std::optional<SkColor> icon_color;
    // Whether the icon image is just a mask used to generate a monochrome icon.
    bool icon_is_monochrome;
    int64_t user_id;
    proto::AppStreamabilityStatus app_streamability_status;
  };

  // Interaction behavior for integration with other features.
  enum class InteractionBehavior {
    // Default value. No interactions available.
    kNone,

    // Notification can be opened.
    kOpenable,
  };

  // Interaction behavior for integration with other features.
  enum class ActionType {
    // Default value. No interactions available.
    kNone,

    // User can click the reply button for the conversation type notification.
    kInlineReply,

    // User can click answer button for the incoming call notification.
    kAnswer,

    // User can click decline button for the incoming call notification.
    kDecline,

    // User can click hang up button for the ongoing call notification.
    kHangup,
  };

  enum class Category {
    // Default value..
    kNone,

    // User can click the reply button for the conversation type notification.
    kConversation,

    // The incoming call notification with answer and decline action buttons.
    // User can click the answer button to open the App streaming window to
    // answer the call and click the decline button to decline call directly.
    kIncomingCall,

    // The ongoing call notification with a hangup action button. User can
    // click on the body of notification to open the App streaming window to
    // resume the call.
    kOngoingCall,

    // The Screening call notification with a screening call action button.
    kScreenCall,
  };

  // Notification importance; for more details, see
  // https://developer.android.com/reference/android/app/NotificationManager.
  enum class Importance {
    // Older versions of Android do not specify an importance level.
    kUnspecified,

    // Does not show in the Android notification shade.
    kNone,

    // Shows in the Android notification shade, below the fold.
    kMin,

    // Shows in the Android notification shade and potentially status bar, but
    // is not audibly intrusive.
    kLow,

    // Shows in the Android notification shade and status bar and makes noise,
    // but does not visually intrude.
    kDefault,

    // Shows in the Android notification shade and status bar, makes noise, and
    // "peeks" down onto the screen when received.
    kHigh
  };

  // Note: A notification should include at least one of |title|,
  // |text_content|, and |shared_image| so that it can be rendered in the UI.
  Notification(
      int64_t id,
      const AppMetadata& app_metadata,
      const base::Time& timestamp,
      Importance importance,
      Notification::Category category,
      const base::flat_map<Notification::ActionType, int64_t>& action_id_map,
      InteractionBehavior interaction_behavior,
      const std::optional<std::u16string>& title = std::nullopt,
      const std::optional<std::u16string>& text_content = std::nullopt,
      const std::optional<gfx::Image>& shared_image = std::nullopt,
      const std::optional<gfx::Image>& contact_image = std::nullopt);
  Notification(const Notification& other);
  ~Notification();

  bool operator<(const Notification& other) const;
  bool operator==(const Notification& other) const;
  bool operator!=(const Notification& other) const;

  int64_t id() const { return id_; }
  const AppMetadata& app_metadata() const { return app_metadata_; }
  base::Time timestamp() const { return timestamp_; }
  Importance importance() const { return importance_; }
  Notification::Category category() const { return category_; }
  base::flat_map<Notification::ActionType, int64_t> action_id_map() const {
    return action_id_map_;
  }
  InteractionBehavior interaction_behavior() const {
    return interaction_behavior_;
  }
  const std::optional<std::u16string>& title() const { return title_; }
  const std::optional<std::u16string>& text_content() const {
    return text_content_;
  }
  const std::optional<gfx::Image>& shared_image() const {
    return shared_image_;
  }
  const std::optional<gfx::Image>& contact_image() const {
    return contact_image_;
  }

 private:
  int64_t id_;
  AppMetadata app_metadata_;
  base::Time timestamp_;
  Importance importance_;
  Notification::Category category_;
  base::flat_map<Notification::ActionType, int64_t> action_id_map_;
  InteractionBehavior interaction_behavior_;
  std::optional<std::u16string> title_;
  std::optional<std::u16string> text_content_;
  std::optional<gfx::Image> shared_image_;
  std::optional<gfx::Image> contact_image_;
};

std::ostream& operator<<(std::ostream& stream,
                         const Notification::AppMetadata& app_metadata);
std::ostream& operator<<(std::ostream& stream,
                         Notification::Importance importance);
std::ostream& operator<<(std::ostream& stream,
                         const Notification& notification);
std::ostream& operator<<(std::ostream& stream,
                         const Notification::InteractionBehavior behavior);
std::ostream& operator<<(std::ostream& stream,
                         const Notification::Category category);

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_H_
