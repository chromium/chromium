// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"  // nogncheck

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace updates {
class UpdateNotificationService;
}  // namespace updates

class UpdateNotificationServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static UpdateNotificationServiceFactory* GetInstance();
  static updates::UpdateNotificationService* GetForKey(SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<UpdateNotificationServiceFactory>;

  UpdateNotificationServiceFactory();
  ~UpdateNotificationServiceFactory() override;

  // SimpleKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationServiceFactory);
};

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_FACTORY_H_
