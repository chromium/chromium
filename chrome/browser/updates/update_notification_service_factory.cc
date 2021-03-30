// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"  // nogncheck
#include "chrome/browser/profiles/incognito_helpers.h"  // nogncheck
#include "chrome/browser/profiles/profile_key.h"        // nogncheck
#include "chrome/browser/updates/internal/update_notification_service_impl.h"
#include "chrome/browser/updates/update_notification_config.h"  // nogncheck
#include "chrome/browser/updates/update_notification_service_bridge.h"  // nogncheck
#include "chrome/browser/updates/update_notification_service_bridge_android.h"  // nogncheck
#include "components/keyed_service/core/simple_dependency_manager.h"

// static
UpdateNotificationServiceFactory*
UpdateNotificationServiceFactory::GetInstance() {
  return base::Singleton<UpdateNotificationServiceFactory>::get();
}

// static
updates::UpdateNotificationService* UpdateNotificationServiceFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<updates::UpdateNotificationService*>(
      GetInstance()->GetServiceForKey(key, true /* create */));
}

UpdateNotificationServiceFactory::UpdateNotificationServiceFactory()
    : SimpleKeyedServiceFactory("updates::UpdateNotificationService",
                                SimpleDependencyManager::GetInstance()) {
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
}

UpdateNotificationServiceFactory::~UpdateNotificationServiceFactory() = default;

std::unique_ptr<KeyedService>
UpdateNotificationServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  auto* profile_key = ProfileKey::FromSimpleFactoryKey(key);

  auto* schedule_service =
      NotificationScheduleServiceFactory::GetForKey(profile_key);
  auto config = updates::UpdateNotificationConfig::CreateFromFinch();
  auto bridge =
      std::make_unique<updates::UpdateNotificationServiceBridgeAndroid>();
  return std::make_unique<updates::UpdateNotificationServiceImpl>(
      schedule_service, std::move(config), std::move(bridge),
      base::DefaultClock::GetInstance());
}

SimpleFactoryKey* UpdateNotificationServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  return profile_key->GetOriginalKey();
}
