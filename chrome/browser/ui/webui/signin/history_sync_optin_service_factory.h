// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class HistorySyncOptinService;
class Profile;

class HistorySyncOptinServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static HistorySyncOptinService* GetForProfile(Profile* profile);
  static HistorySyncOptinServiceFactory* GetInstance();
  HistorySyncOptinServiceFactory(const HistorySyncOptinServiceFactory&) =
      delete;
  HistorySyncOptinServiceFactory& operator=(
      const HistorySyncOptinServiceFactory&) = delete;

 private:
  friend base::NoDestructor<HistorySyncOptinServiceFactory>;
  HistorySyncOptinServiceFactory();
  ~HistorySyncOptinServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};
#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_FACTORY_H_
