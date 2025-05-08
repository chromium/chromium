// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_IMMEDIATE_REQUEST_RATE_LIMITER_FACTORY_H_
#define CHROME_BROWSER_WEBAUTHN_IMMEDIATE_REQUEST_RATE_LIMITER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace webauthn {
class ImmediateRequestRateLimiter;
}

class ImmediateRequestRateLimiterFactory : public ProfileKeyedServiceFactory {
 public:
  static webauthn::ImmediateRequestRateLimiter* GetForProfile(
      content::BrowserContext* context);
  static ImmediateRequestRateLimiterFactory* GetInstance();

  ImmediateRequestRateLimiterFactory(
      const ImmediateRequestRateLimiterFactory&) = delete;
  ImmediateRequestRateLimiterFactory& operator=(
      const ImmediateRequestRateLimiterFactory&) = delete;

 private:
  friend class base::NoDestructor<ImmediateRequestRateLimiterFactory>;

  ImmediateRequestRateLimiterFactory();
  ~ImmediateRequestRateLimiterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_WEBAUTHN_IMMEDIATE_REQUEST_RATE_LIMITER_FACTORY_H_
