// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_H_

#include <stdint.h>
#include <ostream>
#include <string>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"

namespace chromeos {
namespace phonehub {

// A notification generated on the phone, whose contents are transferred to
// Chrome OS via a Phone Hub connection. Notifications in Phone Hub support
// inline reply and images.
class Notification {
 public:
  // Describes the app which generates a notification.
  struct AppMetadata {
    AppMetadata(const std::u16string& visible_app_name,
                const std::string& package_name,
                const gfx::Image& icon);
    AppMetadata(const AppMetadata& other);
    AppMetadata& operator=(const AppMetadata& other);

    bool operator==(const AppMetadata& other) const;
    bool operator!=(const AppMetadata& other) const;

    std::u16string visible_app_name;
    std::string package_name;
    gfx::Image icon;
  };

  // Interaction behavior for integration with other features.
  enum class InteractionBehavior {
    // Default value. No interactions available.
    kNone,

    // Notification can be opened.
    kOpenable
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
      int64_t inline_reply_id,
      InteractionBehavior interaction_behavior,
      const absl::optional<std::u16string>& title = absl::nullopt,
      const absl::optional<std::u16string>& text_content = absl::nullopt,
      const absl::optional<gfx::Image>& shared_image = absl::nullopt,
      const absl::optional<gfx::Image>& contact_image = absl::nullopt);
  Notification(const Notification& other);
  ~Notification();

  bool operator<(const Notification& other) const;
  bool operator==(const Notification& other) const;
  bool operator!=(const Notification& other) const;

  int64_t id() const { return id_; }
  const AppMetadata& app_metadata() const { return app_metadata_; }
  base::Time timestamp() const { return timestamp_; }
  Importance importance() const { return importance_; }
  int64_t inline_reply_id() const { return inline_reply_id_; }
  InteractionBehavior interaction_behavior() const {
    return interaction_behavior_;
  }
  const absl::optional<std::u16string>& title() const { return title_; }
  const absl::optional<std::u16string>& text_content() const {
    return text_content_;
  }
  const absl::optional<gfx::Image>& shared_image() const {
    return shared_image_;
  }
  const absl::optional<gfx::Image>& contact_image() const {
    return contact_image_;
  }

 private:
  int64_t id_;
  AppMetadata app_metadata_;
  base::Time timestamp_;
  Importance importance_;
  int64_t inline_reply_id_;
  InteractionBehavior interaction_behavior_;
  absl::optional<std::u16string> title_;
  absl::optional<std::u16string> text_content_;
  absl::optional<gfx::Image> shared_image_;
  absl::optional<gfx::Image> contact_image_;
};

std::ostream& operator<<(std::ostream& stream,
                         const Notification::AppMetadata& app_metadata);
std::ostream& operator<<(std::ostream& stream,
                         Notification::Importance importance);
std::ostream& operator<<(std::ostream& stream,
                         const Notification& notification);
std::ostream& operator<<(std::ostream& stream,
                         const Notification::InteractionBehavior behavior);
}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_H_
