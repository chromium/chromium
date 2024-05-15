// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/mojom/mac_notifications_mojom_traits.h"

namespace mojo {

// static
mac_notifications::mojom::NotificationOperation
EnumTraits<mac_notifications::mojom::NotificationOperation,
           NotificationOperation>::ToMojom(NotificationOperation input) {
  switch (input) {
    case NotificationOperation::kClick:
      return mac_notifications::mojom::NotificationOperation::kClick;
    case NotificationOperation::kClose:
      return mac_notifications::mojom::NotificationOperation::kClose;
    case NotificationOperation::kSettings:
      return mac_notifications::mojom::NotificationOperation::kSettings;
    case NotificationOperation::kDisablePermission:
      // This is not supported in macOS notifications.
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return mac_notifications::mojom::NotificationOperation::kClick;
}

// static
bool EnumTraits<mac_notifications::mojom::NotificationOperation,
                NotificationOperation>::
    FromMojom(mac_notifications::mojom::NotificationOperation input,
              NotificationOperation* output) {
  switch (input) {
    case mac_notifications::mojom::NotificationOperation::kClick:
      *output = NotificationOperation::kClick;
      return true;
    case mac_notifications::mojom::NotificationOperation::kClose:
      *output = NotificationOperation::kClose;
      return true;
    case mac_notifications::mojom::NotificationOperation::kSettings:
      *output = NotificationOperation::kSettings;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
