// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

class NotificationPermissionsReviewServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static NotificationPermissionsReviewServiceFactory* GetInstance();

  static NotificationPermissionsReviewService* GetForProfile(Profile* profile);

  // Non-copyable, non-moveable.
  NotificationPermissionsReviewServiceFactory(
      const NotificationPermissionsReviewServiceFactory&) = delete;
  NotificationPermissionsReviewServiceFactory& operator=(
      const NotificationPermissionsReviewServiceFactory&) = delete;

 private:
  friend base::NoDestructor<NotificationPermissionsReviewServiceFactory>;

  NotificationPermissionsReviewServiceFactory();
  ~NotificationPermissionsReviewServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

#if BUILDFLAG(IS_ANDROID)
  bool ServiceIsCreatedWithBrowserContext() const override;
#endif  // BUILDFLAG(ANDROID)
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_FACTORY_H_
