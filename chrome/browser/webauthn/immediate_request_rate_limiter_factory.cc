// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/immediate_request_rate_limiter_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/webauthn/core/browser/immediate_request_rate_limiter.h"

// static
webauthn::ImmediateRequestRateLimiter*
ImmediateRequestRateLimiterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<webauthn::ImmediateRequestRateLimiter*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ImmediateRequestRateLimiterFactory*
ImmediateRequestRateLimiterFactory::GetInstance() {
  static base::NoDestructor<ImmediateRequestRateLimiterFactory> instance;
  return instance.get();
}

ImmediateRequestRateLimiterFactory::ImmediateRequestRateLimiterFactory()
    : ProfileKeyedServiceFactory(
          "ImmediateRequestRateLimiter",
          ProfileSelections::BuildForRegularProfile()) {}

ImmediateRequestRateLimiterFactory::~ImmediateRequestRateLimiterFactory() =
    default;

std::unique_ptr<KeyedService>
ImmediateRequestRateLimiterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<webauthn::ImmediateRequestRateLimiter>();
}
