// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/mojom/mac_notifications_mojom_traits.h"

#include "base/notreached.h"

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
    case NotificationOperation::kReportAsSafe:
    case NotificationOperation::kReportWarnedAsSpam:
    case NotificationOperation::kReportUnwarnedAsSpam:
    case NotificationOperation::kShowOriginalNotification:
      // This is not supported in macOS notifications.
      break;
  }
  NOTREACHED();
}

// static
NotificationOperation
EnumTraits<mac_notifications::mojom::NotificationOperation,
           NotificationOperation>::
    FromMojom(mac_notifications::mojom::NotificationOperation input) {
  switch (input) {
    case mac_notifications::mojom::NotificationOperation::kClick:
      return NotificationOperation::kClick;
    case mac_notifications::mojom::NotificationOperation::kClose:
      return NotificationOperation::kClose;
    case mac_notifications::mojom::NotificationOperation::kSettings:
      return NotificationOperation::kSettings;
  }
  NOTREACHED();
}

}  // namespace mojo
