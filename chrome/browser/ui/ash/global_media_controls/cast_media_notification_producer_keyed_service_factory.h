// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_KEYED_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class CastMediaNotificationProducerKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  CastMediaNotificationProducerKeyedServiceFactory();
  CastMediaNotificationProducerKeyedServiceFactory(
      const CastMediaNotificationProducerKeyedServiceFactory&) = delete;
  CastMediaNotificationProducerKeyedServiceFactory& operator=(
      const CastMediaNotificationProducerKeyedServiceFactory&) = delete;
  ~CastMediaNotificationProducerKeyedServiceFactory() override;

  static CastMediaNotificationProducerKeyedServiceFactory* GetInstance();

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_KEYED_SERVICE_FACTORY_H_
