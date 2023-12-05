// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_SERVICES_AUTH_TOKEN_PUBLIC_CPP_AUTH_TOKEN_SERVICE_FACTORY_H_
#define COMPONENTS_RECORD_REPLAY_SERVICES_AUTH_TOKEN_PUBLIC_CPP_AUTH_TOKEN_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/record_replay/services/auth_token/public/cpp/auth_token_service.h"

namespace content {
class BrowserContext;
}

namespace auth_token {

class RecordReplayAuthTokenService;

// Factory to get or create an instance of RecordReplayAuthTokenService for a
// BrowserContext.
class RecordReplayAuthTokenServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RecordReplayAuthTokenService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<RecordReplayAuthTokenServiceFactory>;
  static RecordReplayAuthTokenServiceFactory* GetInstance();

  RecordReplayAuthTokenServiceFactory();
  ~RecordReplayAuthTokenServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace auth_token

#endif  // COMPONENTS_RECORD_REPLAY_SERVICES_AUTH_TOKEN_PUBLIC_CPP_AUTH_TOKEN_SERVICE_FACTORY_H_
