// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/safety_hub/notification_wrapper_android.h"
#endif

// static
RevokedPermissionsOSNotificationDisplayManager*
RevokedPermissionsOSNotificationDisplayManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RevokedPermissionsOSNotificationDisplayManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RevokedPermissionsOSNotificationDisplayManagerFactory*
RevokedPermissionsOSNotificationDisplayManagerFactory::GetInstance() {
  static base::NoDestructor<
      RevokedPermissionsOSNotificationDisplayManagerFactory>
      instance;
  return instance.get();
}

RevokedPermissionsOSNotificationDisplayManagerFactory::
    RevokedPermissionsOSNotificationDisplayManagerFactory()
    : ProfileKeyedServiceFactory(
          "RevokedPermissionsOSNotificationDisplayManager",
          ProfileSelections::BuildForRegularProfile()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

RevokedPermissionsOSNotificationDisplayManagerFactory::
    ~RevokedPermissionsOSNotificationDisplayManagerFactory() = default;

std::unique_ptr<KeyedService>
RevokedPermissionsOSNotificationDisplayManagerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<RevokedPermissionsOSNotificationDisplayManager>(
      HostContentSettingsMapFactory::GetForProfile(profile),
// Notification is only displayed on Android.
#if BUILDFLAG(IS_ANDROID)
      std::make_unique<NotificationWrapperAndroid>()
#else
      nullptr
#endif
  );
}
