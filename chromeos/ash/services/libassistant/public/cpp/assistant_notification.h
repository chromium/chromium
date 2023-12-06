// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_NOTIFICATION_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_NOTIFICATION_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace ash::assistant {

// Models a notification button.
struct COMPONENT_EXPORT(LIBASSISTANT_PUBLIC_STRUCTS)
    AssistantNotificationButton {
  // Display text of the button.
  std::string label;

  // Optional URL to open when the tap action is invoked on the button.
  GURL action_url;

  // If |true|, the associated notification will be removed on button click.
  bool remove_notification_on_click = true;
};

// Enumeration of possible notification priorities.
// NOTE: This enum maps to a subset of message_center::NotificationPriority.
enum class AssistantNotificationPriority {
  kLow,      // See message_center::NotificationPriority::LOW_PRIORITY.
  kDefault,  // See message_center::NotificationPriority::DEFAULT_PRIORITY.
  kHigh,     // See message_center::NotificationPriority::HIGH_PRIORITY.
};

// Models an Assistant notification.
struct COMPONENT_EXPORT(LIBASSISTANT_PUBLIC_STRUCTS) AssistantNotification {
  AssistantNotification();
  AssistantNotification(const AssistantNotification&);
  AssistantNotification& operator=(const AssistantNotification&);
  AssistantNotification(AssistantNotification&&);
  AssistantNotification& operator=(AssistantNotification&&);
  virtual ~AssistantNotification();

  // Title of the notification.
  std::string title;

  // Body text of the notification.
  std::string message;

  // Optional URL to open when the tap action is invoked on the notification
  // main UI.
  GURL action_url;

  // List of buttons in the notification.
  std::vector<AssistantNotificationButton> buttons;

  // An id that uniquely identifies a notification on the client.
  std::string client_id;

  // An id that uniquely identifies a notification on the server.
  std::string server_id;

  // Used to fetch notification contents.
  std::string consistency_token;
  std::string opaque_token;

  // Key that can be used to group multiple notifications together.
  std::string grouping_key;

  // Obfuscated Gaia id of the intended recipient of the user.
  std::string obfuscated_gaia_id;

  // Priority for the notification. This affects whether or not the notification
  // will pop up and/or have the capability to wake the display if it was off.
  AssistantNotificationPriority priority{
      AssistantNotificationPriority::kDefault};

  // When the notification should expire.
  // Expressed as milliseconds since Unix Epoch.
  std::optional<base::Time> expiry_time;

  // If |true|, the notification will be removed on click.
  bool remove_on_click = true;

  // If |true|, the notification state (e.g. its popup and read status) will be
  // reset so as to renotify the user of this notification.
  // See `message_center::Notification::renotify()`.
  bool renotify = false;

  // If |true|, the user can't remove the notification.
  bool is_pinned = false;

  // If |true|, this notification was sent from the server. Clicking a
  // notification that was sent from the server may result in a request to the
  // server to retrieve the notification payload. One example of this would be
  // notifications triggered by an Assistant reminder.
  bool from_server = false;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_NOTIFICATION_H_
