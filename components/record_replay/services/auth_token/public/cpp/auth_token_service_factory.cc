// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/services/auth_token/public/cpp/auth_token_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace auth_token {

// static
auth_token::RecordReplayAuthTokenService*
RecordReplayAuthTokenServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<auth_token::RecordReplayAuthTokenService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
RecordReplayAuthTokenServiceFactory* RecordReplayAuthTokenServiceFactory::GetInstance() {
  static base::NoDestructor<RecordReplayAuthTokenServiceFactory> instance;
  return instance.get();
}

RecordReplayAuthTokenServiceFactory::RecordReplayAuthTokenServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "RecordReplayAuthTokenService",
          BrowserContextDependencyManager::GetInstance()) {}

RecordReplayAuthTokenServiceFactory::~RecordReplayAuthTokenServiceFactory() = default;

KeyedService* RecordReplayAuthTokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* /*context*/) const {
  return new auth_token::RecordReplayAuthTokenService();
}

// Incognito profiles should use their own instance.
content::BrowserContext* RecordReplayAuthTokenServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace auth_token
