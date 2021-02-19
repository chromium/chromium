// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/mojom/mac_notifications_mojom_traits.h"

namespace mojo {

// static
mac_notifications::mojom::NotificationOperation
EnumTraits<mac_notifications::mojom::NotificationOperation,
           NotificationOperation>::ToMojom(NotificationOperation input) {
  switch (input) {
    case NotificationOperation::NOTIFICATION_CLICK:
      return mac_notifications::mojom::NotificationOperation::kClick;
    case NotificationOperation::NOTIFICATION_CLOSE:
      return mac_notifications::mojom::NotificationOperation::kClose;
    case NotificationOperation::NOTIFICATION_SETTINGS:
      return mac_notifications::mojom::NotificationOperation::kSettings;
    case NotificationOperation::NOTIFICATION_DISABLE_PERMISSION:
      // This is not supported in macOS notifications.
      break;
  }
  NOTREACHED();
  return mac_notifications::mojom::NotificationOperation::kClick;
}

// static
bool EnumTraits<mac_notifications::mojom::NotificationOperation,
                NotificationOperation>::
    FromMojom(mac_notifications::mojom::NotificationOperation input,
              NotificationOperation* output) {
  switch (input) {
    case mac_notifications::mojom::NotificationOperation::kClick:
      *output = NotificationOperation::NOTIFICATION_CLICK;
      return true;
    case mac_notifications::mojom::NotificationOperation::kClose:
      *output = NotificationOperation::NOTIFICATION_CLOSE;
      return true;
    case mac_notifications::mojom::NotificationOperation::kSettings:
      *output = NotificationOperation::NOTIFICATION_SETTINGS;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
