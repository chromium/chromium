// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_device_salt/media_device_salt_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace media_device_salt {

MediaDeviceSaltServiceFactory* MediaDeviceSaltServiceFactory::GetInstance() {
  static base::NoDestructor<MediaDeviceSaltServiceFactory> factory;
  return factory.get();
}

MediaDeviceSaltService* MediaDeviceSaltServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<MediaDeviceSaltService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

MediaDeviceSaltServiceFactory::MediaDeviceSaltServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaDeviceSaltServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
MediaDeviceSaltServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<MediaDeviceSaltService>(
      user_prefs::UserPrefs::Get(context));
}

}  // namespace media_device_salt
