// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class TrustSafetySentimentServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static TrustSafetySentimentServiceFactory* GetInstance();
  static TrustSafetySentimentService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<
      TrustSafetySentimentServiceFactory>;

  TrustSafetySentimentServiceFactory();
  ~TrustSafetySentimentServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_FACTORY_H_
