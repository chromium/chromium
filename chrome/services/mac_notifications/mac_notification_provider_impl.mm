// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_provider_impl.h"

#import <Foundation/NSUserNotification.h>
#import <UserNotifications/UserNotifications.h>

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#import "chrome/services/mac_notifications/mac_notification_service_ns.h"
#import "chrome/services/mac_notifications/mac_notification_service_un.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  // Use the UNNotification API if available and enabled.
  if (@available(macOS 10.14, *)) {
    if (base::FeatureList::IsEnabled(features::kNewMacNotificationAPI)) {
      service_ = std::make_unique<MacNotificationServiceUN>(
          std::move(service), std::move(handler),
          [UNUserNotificationCenter currentNotificationCenter]);
      return;
    }
  }

  service_ = std::make_unique<MacNotificationServiceNS>(
      std::move(service), std::move(handler),
      [NSUserNotificationCenter defaultUserNotificationCenter]);
}

}  // namespace mac_notifications
