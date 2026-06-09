// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_provider_impl.h"

#import <Foundation/NSUserNotification.h>
#import <UserNotifications/UserNotifications.h>

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/common/chrome_features.h"
#import "chrome/services/mac_notifications/mac_notification_service_ns.h"
#import "chrome/services/mac_notifications/mac_notification_service_un.h"

namespace mac_notifications {

MacNotificationProviderImpl::MacNotificationProviderImpl() = default;

MacNotificationProviderImpl::MacNotificationProviderImpl(
    mojo::PendingReceiver<mojom::MacNotificationProvider> binding)
    : binding_(this, std::move(binding)) {}

MacNotificationProviderImpl::~MacNotificationProviderImpl() = default;

void MacNotificationProviderImpl::BindNotificationService(
    mojo::PendingReceiver<mojom::MacNotificationService> service,
    mojo::PendingRemote<mojom::MacNotificationActionHandler> handler) {
  DCHECK(!service_);

// MacNotificationServiceNS implements the Chromium interface to the
// NSUserNotificationCenter deprecated API. It is in the process of being
// replaced by UNNotification, above, and warnings about its deprecation are not
// helpful. https://crbug.com/40148499
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

  service_ = std::make_unique<MacNotificationServiceNS>(
      std::move(service), std::move(handler),
      [NSUserNotificationCenter defaultUserNotificationCenter]);

#pragma clang diagnostic pop
}

}  // namespace mac_notifications
