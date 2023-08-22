// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace password_manager {
class PasswordChangeSuccessTracker;

// Creates instances of |PasswordChangeSuccessTracker| per |BrowserContext|.
class PasswordChangeSuccessTrackerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  PasswordChangeSuccessTrackerFactory();
  ~PasswordChangeSuccessTrackerFactory() override;

  static PasswordChangeSuccessTrackerFactory* GetInstance();
  static PasswordChangeSuccessTracker* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_
