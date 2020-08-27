// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_H_

#include <stdint.h>
#include <ostream>
#include <string>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
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
    AppMetadata(const base::string16& visible_app_name,
                const std::string& package_name,
                const gfx::Image& icon);
    AppMetadata(const AppMetadata& other);

    bool operator==(const AppMetadata& other) const;
    bool operator!=(const AppMetadata& other) const;

    base::string16 visible_app_name;
    std::string package_name;
    gfx::Image icon;
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
      const base::Optional<base::string16>& title = base::nullopt,
      const base::Optional<base::string16>& text_content = base::nullopt,
      const base::Optional<gfx::Image>& shared_image = base::nullopt,
      const base::Optional<gfx::Image>& contact_image = base::nullopt);
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
  const base::Optional<base::string16>& title() const { return title_; }
  const base::Optional<base::string16>& text_content() const {
    return text_content_;
  }
  const base::Optional<gfx::Image>& shared_image() const {
    return shared_image_;
  }
  const base::Optional<gfx::Image>& contact_image() const {
    return contact_image_;
  }

 private:
  int64_t id_;
  AppMetadata app_metadata_;
  base::Time timestamp_;
  Importance importance_;
  int64_t inline_reply_id_;
  base::Optional<base::string16> title_;
  base::Optional<base::string16> text_content_;
  base::Optional<gfx::Image> shared_image_;
  base::Optional<gfx::Image> contact_image_;
};

std::ostream& operator<<(std::ostream& stream,
                         const Notification::AppMetadata& app_metadata);
std::ostream& operator<<(std::ostream& stream,
                         Notification::Importance importance);
std::ostream& operator<<(std::ostream& stream,
                         const Notification& notification);

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_H_
