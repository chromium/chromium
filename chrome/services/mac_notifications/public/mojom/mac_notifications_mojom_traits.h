// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_MOJOM_MAC_NOTIFICATIONS_MOJOM_TRAITS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_MOJOM_MAC_NOTIFICATIONS_MOJOM_TRAITS_H_

#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"

namespace mojo {

template <>
struct EnumTraits<mac_notifications::mojom::NotificationOperation,
                  NotificationOperation> {
  static mac_notifications::mojom::NotificationOperation ToMojom(
      NotificationOperation input);
  static bool FromMojom(mac_notifications::mojom::NotificationOperation input,
                        NotificationOperation* output);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_MOJOM_MAC_NOTIFICATIONS_MOJOM_TRAITS_H_
