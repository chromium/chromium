// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

class TrustSafetySentimentServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TrustSafetySentimentServiceFactory* GetInstance();
  static TrustSafetySentimentService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<TrustSafetySentimentServiceFactory>;

  TrustSafetySentimentServiceFactory();
  ~TrustSafetySentimentServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_FACTORY_H_
